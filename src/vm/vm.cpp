#include "global-manager.hpp"
#include "module-manager.hpp"
#include "native-manager.hpp"
#include "operations.hpp"
#include "script.hpp"
#include "type-conversion.hpp"

using namespace std::literals;

struct _vm : public utils::Object {};

namespace vm {

class VM final : public _vm {
public:
    VM() : _moduleManager(_cache), _globalManager(_cache), _nativeManager(_cache) {
        _cache.strTrue = _cache.pool.insert("true");
        _cache.strFalse = _cache.pool.insert("false");
        for (size_t i = 0; i < _cache.strInts.size(); ++i) {
            _cache.strInts[i] = _cache.pool.insert(std::to_string(i));
        }
        auto _ = _threads.obtain();
    }

    [[nodiscard]] vm_script_h loadScript(vm_script_h script, bool forceLoad) noexcept {
        if (auto module = _moduleManager.getModule(script)) {
            if (forceLoad) {
                _globalManager.addGlobals(script, module->bytecode());
                _nativeManager.addNatives(module->bytecode());
            }
        }
        return script;
    }

    [[nodiscard]] vm_script_h loadScriptFromMemory(std::span<const vm_char_t> code) noexcept {
        try {
            auto [script, loaded] = _moduleManager.loadScript(code);
            return loadScript(script, loaded);
        } catch (const std::bad_alloc&) {
            return Undefined;
        }
    }

    [[nodiscard]] vm_script_h loadScriptFromFile(vm_utf8_t file) noexcept {
        try {
            auto [script, loaded] = _moduleManager.loadScript(file, _importDirs);
            return loadScript(script, loaded);
        } catch (const std::bad_alloc&) {
            return Undefined;
        }
    }

    [[nodiscard]] vm_thread_h createThread(vm_script_h script) noexcept {
        try {
            if (auto scriptPtr = _moduleManager.getModule(script)) {
                auto [handle, thread] = _threads.obtainAndAccess();
                _threadHandles.push_back(handle);
                auto bytecode = scriptPtr->bytecode();
                thread->state = ThreadState::Running;
                thread->script = script;
                thread->stackPos = utils::read<vm_script_export_t>(bytecode).root_frame_slots;
                thread->frameCount = 1;
                return handle;
            }
        } catch (const std::bad_alloc&) {
        }
        return Undefined;
    }

    void destroyThread(vm_thread_h thread) noexcept {
        // TODO: not implemented
    }

    [[nodiscard]] const vm_value_t* getArg(vm_thread_h thread, vm_uint16_t index) const noexcept {
        if (const auto threadPtr = _threads.access(thread)) {
            const auto native = _nativeManager.func(threadPtr->currentNative);
            assert(native);
            const auto argBase = threadPtr->stackPos - native->paramCount;
            if (index < native->paramCount) {
                return &threadPtr->stack[argBase + index];
            }
        }
        return nullptr;
    }

    void setResult(vm_thread_h thread, vm_uint16_t index, const vm_value_t& value) {
        if (const auto threadPtr = _threads.access(thread)) {
            const auto native = _nativeManager.func(threadPtr->currentNative);
            assert(native);
            threadPtr->stack[threadPtr->stackPos + index] = value;
        }
    }

    [[nodiscard]] std::string_view getString(vm_string_h str) const noexcept { return _cache.pool[str]; }

    [[nodiscard]] vm_string_h internString(vm_utf8_t str) noexcept { return _cache.pool.insert(str); }

    [[nodiscard]] NativeManager& native() noexcept { return _nativeManager; }

    void getImportDirs(vm_uint32_t& dirCount, vm_utf8_t* dirs) {
        if (dirs) {
            dirCount = std::min(dirCount, static_cast<vm_uint32_t>(_importStrDirs.size()));
            for (vm_uint32_t i = 0; i < dirCount; ++i) {
                dirs[i] = _importStrDirs[i].c_str();
            }
        } else {
            dirCount = (vm_uint32_t)_importStrDirs.size();
        }
    }

    void setImportDirs(std::span<const vm_utf8_t> dirs) {
        _importDirs.clear();
        _importDirs.reserve(dirs.size());
        std::copy(dirs.begin(), dirs.end(), std::back_inserter(_importDirs));
        for (auto& dir : _importDirs) {
            dir.make_preferred();
        }

        _importStrDirs.clear();
        _importStrDirs.reserve(_importDirs.size());
        for (const auto& dir : _importDirs) {
            _importStrDirs.emplace_back(dir.string());
        }
    }

    [[nodiscard]] vm_import_callback_t getImporter(vm_data_t* data) {
        if (data) {
            *data = _importerData;
        }
        return _importer;
    }

    void setImporter(vm_import_callback_t callback, vm_data_t data) {
        if (callback) {
            _importer = callback;
            _importerData = data;
        } else {
            _importer = nullptr;
            _importerData = nullptr;
        }
    }

    [[nodiscard]] vm_release_import_callback_t getReleaseImport(vm_data_t* data) {
        if (data) {
            *data = _releaseImportData;
        }
        return _releaseImport;
    }

    void setReleaseImport(vm_release_import_callback_t callback, vm_data_t data) {
        if (callback) {
            _releaseImport = callback;
            _releaseImportData = data;
        } else {
            _releaseImport = nullptr;
            _releaseImportData = nullptr;
        }
    }

    void tick() {
        for (size_t i = 0; i < _threadHandles.size();) {
            auto handle = _threadHandles[i++];
            auto thread = _threads.access(handle);
            if (thread->state == ThreadState::Dead) {
                for (size_t j = 0; j < _threadHandles.size(); ++j) {
                    if (j != i) {
                        auto otherThread = _threads.access(_threadHandles[j]);
                        if (otherThread->waitThread == handle) {
                            otherThread->waitThread = Undefined;
                            otherThread->state = ThreadState::Running;
                        }
                    }
                }
                _threads.release(_threadHandles[--i]);
                if (_threadHandles.size() > 1) {
                    _threadHandles[i] = _threadHandles.back();
                }
                _threadHandles.pop_back();
                continue;
            }
            if (thread->state == ThreadState::Waiting) {
                if (thread->waitTicks > 0) {
                    thread->waitTicks--; // TODO use elapsed, thread->waitTicks -= std::min(thread->waitTicks, elapsed);
                }
                if (thread->waitTicks == 0 && thread->waitThread == Undefined) {
                    thread->state = ThreadState::Running;
                } else if (
                    thread->waitThread != Undefined &&
                    _threads.access(thread->waitThread)->state == ThreadState::Dead) {
                    thread->waitThread = Undefined;
                    thread->state = ThreadState::Running;
                } else {
                    continue;
                }
            }
            if (thread->state == ThreadState::Running) {
                stepThread(thread);
            }
        }
    }

private:
    void stepThread(ScriptThread* thread) {
        auto module = _moduleManager.getModule(thread->script);
        auto text = module->text();
        auto data = module->data();
        while (thread->state == ThreadState::Running && thread->instructionPos < text.size()) {
            const auto opcode = static_cast<vm_opcode_t>(text[thread->instructionPos++]);
            switch (opcode) {
            case VM_OPCODE_NO_OPERATION:
                break;
            case VM_OPCODE_IMPORT: {
                const auto view = utils::readString(text, thread->instructionPos, data);
                if (!_moduleManager.hasModule(view)) {
                    const auto waitThread = import(view);
                    module = _moduleManager.getModule(thread->script);
                    module->modifyOpcode(thread->instructionPos - 5, VM_OPCODE_NO_OPERATION, 0);
                    if (waitThread != Undefined) {
                        thread->state = ThreadState::Waiting;
                        thread->waitThread = waitThread;
                        return;
                    }
                }
                break;
            }
            case VM_OPCODE_PUSH_TRUE:
                thread->stack[thread->stackPos++] = valueFromBool(true);
                break;
            case VM_OPCODE_PUSH_FALSE:
                thread->stack[thread->stackPos++] = valueFromBool(false);
                break;
            case VM_OPCODE_PUSH_INT: {
                const auto value = utils::readInt32(text, thread->instructionPos);
                thread->stack[thread->stackPos++] = valueFromInt32(value);
                break;
            }
            case VM_OPCODE_PUSH_FLOAT: {
                const auto value = utils::readFloat(text, thread->instructionPos);
                thread->stack[thread->stackPos++] = valueFromFloat(value);
                break;
            }
            case VM_OPCODE_PUSH_STRING_FROM_DATA: {
                const auto view = utils::readString(text, thread->instructionPos, data);
                const auto str = _cache.pool.insert(view);
                module->modifyOpcode(thread->instructionPos - 5, VM_OPCODE_PUSH_STRING, str.handle);
                thread->stack[thread->stackPos++] = valueFromString({str});
                break;
            }
            case VM_OPCODE_PUSH_STRING: {
                const auto str = utils::readUint32(text, thread->instructionPos);
                thread->stack[thread->stackPos++] = valueFromString({str});
                break;
            }
            case VM_OPCODE_PUSH_GLOBAL_BY_NAME: {
                const auto name = utils::readString(text, thread->instructionPos, data);
                if (auto slot = _globalManager.resolveVar(name); slot != Undefined) {
                    module->modifyOpcode(thread->instructionPos - 5, VM_OPCODE_PUSH_GLOBAL, slot.handle);
                    thread->stack[thread->stackPos++] = _globalManager.getVar(slot);
                } else {
                    // TODO: log
                }
                break;
            }
            case VM_OPCODE_PUSH_GLOBAL: {
                const auto slot = utils::readUint32(text, thread->instructionPos);
                thread->stack[thread->stackPos++] = _globalManager.getVar({slot});
                break;
            }
            case VM_OPCODE_PUSH_LOCAL: {
                const auto slot = utils::readUint16(text, thread->instructionPos);
                const auto& currentFrame = thread->frames[thread->frameCount - 1];
                const auto localSlot = currentFrame.baseStackPos + slot;
                thread->stack[thread->stackPos++] = thread->stack[localSlot];
                break;
            }
            case VM_OPCODE_POP_GLOBAL_BY_NAME: {
                const auto name = utils::readString(text, thread->instructionPos, data);
                if (auto slot = _globalManager.resolveVar(name); slot != Undefined) {
                    module->modifyOpcode(thread->instructionPos - 5, VM_OPCODE_POP_GLOBAL, slot.handle);
                    _globalManager.setVar(slot, thread->stack[--thread->stackPos]);
                } else {
                    // TODO: log
                }
                break;
            }
            case VM_OPCODE_POP_GLOBAL: {
                const auto slot = utils::readUint32(text, thread->instructionPos);
                _globalManager.setVar({slot}, thread->stack[--thread->stackPos]);
                break;
            }
            case VM_OPCODE_POP_LOCAL: {
                const auto slot = utils::readUint16(text, thread->instructionPos);
                const auto& currentFrame = thread->frames[thread->frameCount - 1];
                const auto val = thread->stack[--thread->stackPos];
                const auto localSlot = currentFrame.baseStackPos + slot;
                thread->stack[localSlot] = val;
                break;
            }
            case VM_OPCODE_POP:
                --thread->stackPos;
                break;
            case VM_OPCODE_BOOL_TO_INT:
                cast<bool, vm_sint32_t>(thread);
                break;
            case VM_OPCODE_BOOL_TO_FLOAT:
                cast<bool, vm_float32_t>(thread);
                break;
            case VM_OPCODE_BOOL_TO_STRING:
                cast<bool, vm_string_h>(thread);
                break;
            case VM_OPCODE_INT_TO_BOOL:
                cast<vm_sint32_t, bool>(thread);
                break;
            case VM_OPCODE_INT_TO_FLOAT:
                cast<vm_sint32_t, vm_float32_t>(thread);
                break;
            case VM_OPCODE_INT_TO_STRING:
                cast<vm_sint32_t, vm_string_h>(thread);
                break;
            case VM_OPCODE_FLOAT_TO_BOOL:
                cast<vm_float32_t, bool>(thread);
                break;
            case VM_OPCODE_FLOAT_TO_INT:
                cast<vm_float32_t, vm_sint32_t>(thread);
                break;
            case VM_OPCODE_FLOAT_TO_STRING:
                cast<vm_float32_t, vm_string_h>(thread);
                break;
            case VM_OPCODE_STRING_TO_BOOL:
                cast<vm_string_h, bool>(thread);
                break;
            case VM_OPCODE_STRING_TO_INT:
                cast<vm_string_h, vm_sint32_t>(thread);
                break;
            case VM_OPCODE_STRING_TO_FLOAT:
                cast<vm_string_h, vm_float32_t>(thread);
                break;
            case VM_OPCODE_BITWISE_NOT:
                operation<BitwiseNot>(thread);
                break;
            case VM_OPCODE_LOGICAL_NOT:
                operation<LogicalNot>(thread);
                break;
            case VM_OPCODE_NUMERIC_NEGATION:
                operation<NumericNegation>(thread);
                break;
            case VM_OPCODE_BITWISE_AND:
                operation<And>(thread);
                break;
            case VM_OPCODE_BITWISE_OR:
                operation<Or>(thread);
                break;
            case VM_OPCODE_BITWISE_XOR:
                operation<Xor>(thread);
                break;
            case VM_OPCODE_LOGIC_SHIFT_LEFT:
                operation<LogicShiftLeft>(thread);
                break;
            case VM_OPCODE_LOGIC_SHIFT_RIGHT:
                operation<LogicShiftRight>(thread);
                break;
            case VM_OPCODE_ARITHMETIC_SHIFT_LEFT:
                operation<ArithmeticShiftLeft>(thread);
                break;
            case VM_OPCODE_ARITHMETIC_SHIFT_RIGHT:
                operation<ArithmeticShiftRight>(thread);
                break;
            case VM_OPCODE_ADD:
                operation<Add>(thread);
                break;
            case VM_OPCODE_SUBTRACT:
                operation<Sub>(thread);
                break;
            case VM_OPCODE_MULTIPLY:
                operation<Mul>(thread);
                break;
            case VM_OPCODE_DIVISION:
                operation<Div>(thread);
                break;
            case VM_OPCODE_MOD:
                operation<Mod>(thread);
                break;
            case VM_OPCODE_EQUAL:
                operation<Equal>(thread);
                break;
            case VM_OPCODE_NOT_EQUAL:
                operation<NotEqual>(thread);
                break;
            case VM_OPCODE_LESS:
                operation<Less>(thread);
                break;
            case VM_OPCODE_LESS_EQUAL:
                operation<LessEqual>(thread);
                break;
            case VM_OPCODE_GREAT:
                operation<Great>(thread);
                break;
            case VM_OPCODE_GREAT_EQUAL:
                operation<GreatEqual>(thread);
                break;
            case VM_OPCODE_CALL_NATIVE_BY_NAME: {
                const auto name = utils::readString(text, thread->instructionPos, data);
                if (auto native = _nativeManager.find(name); native != Undefined) {
                    module->modifyOpcode(thread->instructionPos - 5, VM_OPCODE_CALL_NATIVE, native.handle);
                    const auto func = _nativeManager.func(native);
                    callNative(thread, native, *func);
                } else {
                    // TODO: log
                }
                break;
            }
            case VM_OPCODE_CALL_NATIVE: {
                const auto native = utils::readUint32(text, thread->instructionPos);
                const auto func = _nativeManager.func({native});
                callNative(thread, {native}, *func);
                break;
            }
            case VM_OPCODE_CALL_BY_NAME: {
                const auto name = utils::readString(text, thread->instructionPos, data);
                if (auto slot = _globalManager.resolveFunc(name); slot != Undefined) {
                    module->modifyOpcode(thread->instructionPos - 5, VM_OPCODE_CALL, slot.handle);
                    const auto& func = _globalManager.getFunc(slot);
                    call(thread, func, module);
                    text = module->text();
                    data = module->data();
                } else {
                    // TODO: log
                }
                break;
            }
            case VM_OPCODE_CALL: {
                const auto slot = utils::readUint32(text, thread->instructionPos);
                const auto& func = _globalManager.getFunc({slot});
                call(thread, func, module);
                text = module->text();
                data = module->data();
                break;
            }
            case VM_OPCODE_RETURN: {
                if (thread->frameCount == 0) {
                    // log
                    thread->state = ThreadState::Dead;
                    return;
                }
                vm_value_t retVal;
                const auto& frame = thread->frames[--thread->frameCount];
                if (frame.hasReturnValue) {
                    retVal = thread->stack[--thread->stackPos];
                }
                swapScript(thread, frame.prevScript, module);
                thread->stackPos = frame.baseStackPos;
                if (frame.hasReturnValue) {
                    thread->stack[thread->stackPos++] = retVal;
                }
                thread->instructionPos = frame.returnInstructionPos;
                text = module->text();
                data = module->data();
                break;
            }
            case VM_OPCODE_JUMP:
                thread->instructionPos = utils::readUint32(text, thread->instructionPos);
                break;
            case VM_OPCODE_JUMP_IF_FALSE: {
                const auto target = utils::readUint32(text, thread->instructionPos);
                const auto value = thread->stack[--thread->stackPos];
                if (value.type == VM_VALUE_TYPE_BOOL) {
                    if (!value.b) {
                        thread->instructionPos = target;
                    }
                } else {
                    // TODO: log
                }
                break;
            }
            case VM_OPCODE_YIELD:
                break;
            case VM_OPCODE_WAIT:
                break;
            case VM_OPCODE_HALT:
                thread->state = ThreadState::Dead;
                return;
            default:
                // TODO: add log: [Error] Thread %d: Unknown opcode 0x%02X at IP %u\n", t->id, opcode, t->ip - 1
                thread->state = ThreadState::Dead;
                return;
            }
        }
    }

    vm_thread_h import(std::string_view moduleName) {
        vm_thread_h result = Undefined;
        vm_script_h script = Undefined;
        if (_importer) {
            if (auto bytecode = _importer(this, moduleName.data(), _importerData)) {
                script = loadScriptFromMemory({bytecode->data, bytecode->size});
                if (_releaseImport) {
                    _releaseImport(this, bytecode, _releaseImportData);
                }
            }
        }
        if (script == Undefined) {
            script = loadScriptFromFile(moduleName.data());
        }
        if (script == Undefined) {
            // TODO: log
        }
        return createThread(script);
    }

    void callNative(ScriptThread* thread, vm_native_h nativeHandle, const Native& native) {
        if (thread->stackPos + native.returnCount < StackSize) {
            thread->currentNative = nativeHandle;
            native.func(this, _threads.handle(thread), nativeHandle, native.paramCount, native.data);
            vm_value_t retVal{};
            if (native.returnCount) {
                assert(native.returnCount == 1);
                retVal = thread->stack[thread->stackPos];
            }
            thread->stackPos = thread->stackPos - native.paramCount;
            if (thread->stackPos - native.paramCount) {
                assert(native.returnCount == 1);
                thread->stack[thread->stackPos++] = retVal;
            }
        } else {
            // log
        }
    }

    void call(ScriptThread* thread, const GlobalManager::Func& func, Module*& module) {
        if (thread->frameCount >= MaxFrames) {
            // TODO: add log printf("[VM Error] Call Stack Overflow in thread %d\n", thread->id);
            thread->state = ThreadState::Dead;
            return;
        }
        auto& frame = thread->frames[thread->frameCount++];
        frame.returnInstructionPos = thread->instructionPos;
        frame.prevScript = thread->script;
        frame.argCount = func.paramCount;
        frame.baseStackPos = thread->stackPos - func.paramCount;
        frame.hasReturnValue = func.returnType.has_value();

        swapScript(thread, func.script, module);
        thread->stackPos = frame.baseStackPos + func.frameSlotCount;
        thread->instructionPos = func.offset;
    }

    void swapScript(ScriptThread* thread, vm_script_h script, Module*& module) noexcept {
        if (thread->script != script) {
            module = _moduleManager.getModule(script);
            thread->script = script;
        }
    }

    template <typename From, typename To>
    void cast(ScriptThread* thread) {
        auto value = thread->stack[--thread->stackPos];
        thread->stack[thread->stackPos++] = Cast<From, To>::cast(value, _cache);
    }

    template <typename Op>
    void operation(ScriptThread* thread) noexcept {
        if constexpr (Op::isUnary) {
            const auto a = thread->stack[--thread->stackPos];
            if constexpr (Op::isLogic) {
                thread->stack[thread->stackPos++] = logicOperation<Op>(a, a);
            } else {
                thread->stack[thread->stackPos++] = arithmeticOperation<Op>(a, a);
            }
        } else {
            const auto b = thread->stack[--thread->stackPos];
            const auto a = thread->stack[--thread->stackPos];
            if constexpr (Op::isLogic) {
                thread->stack[thread->stackPos++] = logicOperation<Op>(a, b);
            } else {
                thread->stack[thread->stackPos++] = arithmeticOperation<Op>(a, b);
            }
        }
    }

    template <typename Op>
    [[nodiscard]] vm_value_t logicOperation(const vm_value_t& a, const vm_value_t& b) noexcept {
        if (a.type == b.type && Op::allowed(a.type)) {
            switch (a.type) {
            case VM_VALUE_TYPE_BOOL:
                return valueFromBool(Op::op(a.b, a.b, _cache.pool));
            case VM_VALUE_TYPE_INT:
                return valueFromBool(Op::op(a.i32, b.i32, _cache.pool));
            case VM_VALUE_TYPE_FLOAT:
                return valueFromBool(Op::op(a.f32, b.f32, _cache.pool));
            case VM_VALUE_TYPE_STRING:
                return valueFromBool(Op::op(a.str, b.str, _cache.pool));
            default:
                assert(!"unreachable code");
                break;
            }
        }
        // TODO: log
        return {};
    }

    template <typename Op>
    [[nodiscard]] vm_value_t arithmeticOperation(const vm_value_t& a, const vm_value_t& b) noexcept {
        if (a.type == b.type && Op::allowed(a.type)) {
            switch (a.type) {
            case VM_VALUE_TYPE_BOOL:
                return valueFromBool(Op::op(a.b, a.b, _cache.pool));
            case VM_VALUE_TYPE_INT:
                return valueFromInt32(Op::op(a.i32, b.i32, _cache.pool));
            case VM_VALUE_TYPE_FLOAT:
                return valueFromFloat(Op::op(a.f32, b.f32, _cache.pool));
            case VM_VALUE_TYPE_STRING:
                return valueFromString(Op::op(a.str, b.str, _cache.pool));
            default:
                assert(!"unreachable code");
                break;
            }
        }
        // TODO: log
        return {};
    }

    Cache _cache;
    ModuleManager _moduleManager;
    GlobalManager _globalManager;
    NativeManager _nativeManager;
    ScriptThreadPool _threads;
    std::vector<vm_thread_h> _threadHandles;

    std::vector<std::string> _importStrDirs{};
    std::vector<std::filesystem::path> _importDirs{};
    vm_import_callback_t _importer{};
    vm_release_import_callback_t _releaseImport{};
    vm_data_t _importerData{};
    vm_data_t _releaseImportData{};
};

} // namespace vm

vm_utf8_t vm_result_to_string(vm_result_t result) {
    switch (result) {
    case VM_RESULT_SUCCESS:
        return "no error has occurred";
    case VM_RESULT_ERROR_UNKNOWN:
        return "unknown error";
    case VM_RESULT_ERROR_OUT_OF_MEMORY:
        return "out of memory";
    case VM_RESULT_ERROR_INVALID_ARG:
        return "invalid argument";
    default:
        return "<unknown result>";
    }
}

vm_uint32_t vm_adler_32(const vm_char_t* buffer, vm_uint32_t size) {
    std::span<const vm_char_t> binary{buffer, size};
    auto view = std::views::all(binary);
    return utils::adler32(std::move(view));
}

vm_bool_t vm_validate_checksum(const vm_char_t* buffer, vm_uint32_t size) {
    std::span<const vm_char_t> binary{buffer, size};
    return utils::validateChecksum(binary) ? 1 : 0;
}

vm_bool_t vm_validate_version(const vm_char_t* buffer, vm_uint32_t size) {
    std::span<const vm_char_t> binary{buffer, size};
    return utils::validateVersion(binary, VM_VERSION) ? 1 : 0;
}

vm_uint16_t vm_read_uint16(const vm_char_t* buffer, vm_uint32_t size, vm_uint32_t* offset) {
    if (buffer && offset) {
        const auto result = utils::readUint16({(const vm_uint8_t*)buffer, size}, *offset);
        return result;
    }
    return 0u;
}

vm_uint32_t vm_read_uint32(const vm_char_t* buffer, vm_uint32_t size, vm_uint32_t* offset) {
    if (buffer && offset) {
        const auto result = utils::readUint32({(const vm_uint8_t*)buffer, size}, *offset);
        return result;
    }
    return 0u;
}

vm_sint32_t vm_read_int32(const vm_char_t* buffer, vm_uint32_t size, vm_uint32_t* offset) {
    if (buffer && offset) {
        const auto result = utils::readInt32({(const vm_uint8_t*)buffer, size}, *offset);
        return result;
    }
    return 0u;
}

vm_float32_t vm_read_float(const vm_char_t* buffer, vm_uint32_t size, vm_uint32_t* offset) {
    if (buffer && offset) {
        const auto result = utils::readFloat({(const vm_uint8_t*)buffer, size}, *offset);
        return result;
    }
    return 0u;
}

vm_script_header_t* vm_read_script_header(const vm_char_t* buffer, vm_uint32_t size, vm_script_header_t* header) {
    if (buffer && header) {
        *header = utils::read<vm_script_header_t>(std::span{(const vm_uint8_t*)buffer, size});
    }
    return header;
}

vm_script_table_t* vm_read_script_table(const vm_char_t* buffer, vm_uint32_t size, vm_script_table_t* table) {
    if (buffer && table) {
        *table = utils::read<vm_script_table_t>(std::span{(const vm_uint8_t*)buffer, size});
    }
    return table;
}

vm_script_export_t* vm_read_script_export(const vm_char_t* buffer, vm_uint32_t size, vm_script_export_t* export_table) {
    if (buffer && export_table) {
        *export_table = utils::read<vm_script_export_t>(std::span{(const vm_uint8_t*)buffer, size});
    }
    return export_table;
}

vm_script_export_var_t* vm_read_script_export_var(
    const vm_char_t* buffer, vm_uint32_t size, vm_uint32_t var_index, vm_script_export_var_t* export_var) {
    if (buffer && export_var) {
        const auto maxVars = utils::read<vm_script_export_t>(std::span{(const vm_uint8_t*)buffer, size}).var_count;
        if (var_index < maxVars) {
            *export_var = utils::read<vm_script_export_var_t>(std::span{(const vm_uint8_t*)buffer, size});
        }
    }
    return export_var;
}

vm_script_export_func_t* vm_read_script_export_func(
    const vm_char_t* buffer, vm_uint32_t size, vm_uint32_t func_index, vm_script_export_func_t* export_func) {
    if (buffer && export_func) {
        const auto maxFuncs = utils::read<vm_script_export_t>(std::span{(const vm_uint8_t*)buffer, size}).func_count;
        if (func_index < maxFuncs) {
            *export_func = utils::read<vm_script_export_func_t>(std::span{(const vm_uint8_t*)buffer, size});
        }
    }
    return export_func;
}

vm_script_export_param_t* vm_read_script_export_func_param(
    const vm_char_t* buffer,
    vm_uint32_t size,
    vm_uint32_t func_index,
    vm_uint32_t param_index,
    vm_script_export_param_t* param) {
    if (buffer && param) {
        const auto maxFuncs = utils::read<vm_script_export_t>(std::span{(const vm_uint8_t*)buffer, size}).func_count;
        if (func_index < maxFuncs) {
            const auto maxParams =
                utils::read<vm_script_export_func_t>(std::span{(const vm_uint8_t*)buffer, size}).param_count;
            if (param_index < maxParams) {
                *param = utils::read<vm_script_export_param_t>(
                    std::span{(const vm_uint8_t*)buffer, size}, func_index, param_index);
            }
        }
    }
    return param;
}

vm_script_export_native_t* vm_read_script_export_native(
    const vm_char_t* buffer, vm_uint32_t size, vm_uint32_t native_index, vm_script_export_native_t* export_native) {
    if (buffer && export_native) {
        const auto maxNatives =
            utils::read<vm_script_export_t>(std::span{(const vm_uint8_t*)buffer, size}).native_count;
        if (native_index < maxNatives) {
            *export_native = utils::read<vm_script_export_native_t>(std::span{(const vm_uint8_t*)buffer, size});
        }
    }
    return export_native;
}

vm_script_export_param_t* vm_read_script_export_native_param(
    const vm_char_t* buffer,
    vm_uint32_t size,
    vm_uint32_t native_index,
    vm_uint32_t param_index,
    vm_script_export_param_t* param) {
    if (buffer && param) {
        const auto maxNatives =
            utils::read<vm_script_export_t>(std::span{(const vm_uint8_t*)buffer, size}).native_count;
        if (native_index < maxNatives) {
            const auto maxParams =
                utils::read<vm_script_export_native_t>(std::span{(const vm_uint8_t*)buffer, size}).param_count;
            if (param_index < maxParams) {
                *param = utils::read<utils::NativeParam>(
                             std::span{(const vm_uint8_t*)buffer, size}, native_index, param_index)
                             .param;
            }
        }
    }
    return param;
}

vm_result_t vm_create(vm_t* vm) {
    return vm ? utils::Object::create<vm_result_t, vm::VM>(*vm) : VM_RESULT_ERROR_INVALID_ARG;
}

vm_t vm_reference(vm_t vm) {
    if (vm) {
        vm->as<vm::VM>()->reference();
    }
    return vm;
}

void vm_destroy(vm_t vm) {
    if (vm) {
        vm->as<vm::VM>()->destroy();
    }
}

void vm_tick(vm_t vm) {
    if (vm) {
        vm->as<vm::VM>()->tick();
    }
}

vm_script_h vm_load_script_from_file(vm_t vm, vm_utf8_t file) {
    return vm ? vm->as<vm::VM>()->loadScriptFromFile(file) : Undefined;
}

vm_script_h vm_load_script_from_memory(vm_t vm, const vm_char_t* code, vm_uint32_t code_size) {
    return vm ? vm->as<vm::VM>()->loadScriptFromMemory({code, code_size}) : Undefined;
}

vm_thread_h vm_run_thread(vm_t vm, vm_script_h script) {
    return vm ? vm->as<vm::VM>()->createThread(script) : Undefined;
}

void vm_shutdown_thread(vm_t vm, vm_thread_h thread) {
    if (vm) {
        vm->as<vm::VM>()->destroyThread(thread);
    }
}

vm_value_t vm_thread_get_arg(vm_t vm, vm_thread_h thread, vm_uint32_t index) {
    if (vm) {
        if (auto result = vm->as<vm::VM>()->getArg(thread, index)) {
            return *result;
        }
    }
    return {};
}

void vm_thread_set_result(vm_t vm, vm_thread_h thread, vm_uint32_t index, const vm_value_t* value) {
    if (vm && value) {
        vm->as<vm::VM>()->setResult(thread, index, *value);
    }
}

vm_utf8_t vm_get_string(vm_t vm, vm_string_h string, vm_uint32_t* length) {
    if (vm) {
        auto result = vm->as<vm::VM>()->getString(string);
        if (length) {
            *length = static_cast<vm_uint32_t>(result.length());
        }
        return result.data();
    }
    if (length) {
        *length = 0;
    }
    return "";
}

vm_string_h vm_intern_string(vm_t vm, vm_utf8_t string) {
    return vm ? vm->as<vm::VM>()->internString(string) : Undefined;
}

vm_native_callback_t vm_get_native(vm_t vm, vm_native_h native, vm_data_t* data) {
    if (auto nativePtr = vm->as<vm::VM>()->native().func(native)) {
        if (data) {
            *data = nativePtr->data;
        }
        return nativePtr->func;
    }
    return nullptr;
}

vm_native_h vm_register_native(vm_t vm, vm_utf8_t name, vm_native_callback_t func, vm_data_t data) {
    return vm ? vm->as<vm::VM>()->native().registerFunc(name, func, data) : Undefined;
}

void vm_unregister_native(vm_t vm, vm_native_h native) {
    if (vm) {
        vm->as<vm::VM>()->native().unregisterFunc(native);
    }
}

void vm_get_import_dirs(vm_t vm, vm_uint32_t* dir_count, vm_utf8_t* dirs) {
    if (dir_count) {
        if (vm) {
            vm->as<vm::VM>()->getImportDirs(*dir_count, dirs);
        } else {
            *dir_count = 0;
        }
    }
}

void vm_set_import_dirs(vm_t vm, vm_uint32_t dir_count, const vm_utf8_t* dirs) {
    if (vm) {
        vm->as<vm::VM>()->setImportDirs({dirs, dir_count});
    }
}

vm_import_callback_t vm_get_importer(vm_t vm, vm_data_t* data) {
    return vm ? vm->as<vm::VM>()->getImporter(data) : nullptr;
}

void vm_set_importer(vm_t vm, vm_import_callback_t callback, vm_data_t data) {
    if (vm) {
        vm->as<vm::VM>()->setImporter(callback, data);
    }
}

vm_release_import_callback_t vm_get_release_import(vm_t vm, vm_data_t* data) {
    return vm ? vm->as<vm::VM>()->getReleaseImport(data) : nullptr;
}

void vm_set_release_import(vm_t vm, vm_release_import_callback_t callback, vm_data_t data) {
    if (vm) {
        vm->as<vm::VM>()->setReleaseImport(callback, data);
    }
}
