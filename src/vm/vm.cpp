#include "global-manager.hpp"
#include "module-manager.hpp"
#include "operations.hpp"
#include "script.hpp"
#include "type-conversion.hpp"

using namespace std::literals;

struct _vm : public utils::Object {};

namespace vm {

class VM final : public _vm {
public:
    VM() {
        _cache.strTrue = _cache.pool.insert("true");
        _cache.strFalse = _cache.pool.insert("false");
        for (size_t i = 0; i < _cache.strInts.size(); ++i) {
            _cache.strInts[i] = _cache.pool.insert(std::to_string(i));
        }
    }

    vm_script_h loadScript(vm_script_h handle, bool forceLoad) noexcept {
        auto module = _moduleManager.getModule(handle);
        if (forceLoad) {
            _globalManager.addGlobals(module);
        }
        createThread(handle);
        return handle;
    }

    [[nodiscard]] vm_script_h loadScriptFromMemory(std::span<const vm_char_t> code, bool forceLoad) noexcept {
        auto [script, loaded] = _moduleManager.loadScript(code, forceLoad);
        loadScript(script, loaded);
        return script;
    }

    [[nodiscard]] vm_script_h loadScriptFromFile(vm_utf8_t file, bool forceLoad) noexcept {
        auto [script, loaded] = _moduleManager.loadScript(file, forceLoad);
        loadScript(script, loaded);
        return script;
    }

    void tick() {
        for (size_t i = 0; i < _threadHandles.size();) {
            auto thread = _threads.access(_threadHandles[i++]);
            if (thread->state == ThreadState::Dead) {
                if (_threadHandles.size() > 1) {
                    _threadHandles[--i] = _threadHandles.back();
                }
                _threadHandles.pop_back();
                continue;
            }
            if (thread->state == ThreadState::Waiting) {
                if (thread->waitTicks > 0) {
                    thread->waitTicks--; // TODO use elapsed, thread->waitTicks -= std::min(thread->waitTicks, elapsed);
                }
                if (thread->waitTicks == 0) {
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
    void createThread(vm_script_h script) {
        auto [handle, thread] = _threads.obtainAndAccess();
        _threadHandles.push_back(handle);
        thread->state = ThreadState::Running;
        thread->script = script;
    }

    void stepThread(ScriptThread* thread) {
        auto text = utils::read<utils::SectionText>(_moduleManager.getModule(thread->script)).text;
        auto data = utils::read<utils::SectionData>(_moduleManager.getModule(thread->script)).data;
        while (thread->state == ThreadState::Running && thread->instructionPos < text.size()) {
            const auto opcode = static_cast<vm_opcode_t>(text[thread->instructionPos++]);
            switch (opcode) {
            case VM_OPCODE_NO_OPERATION:
                break;
            case VM_OPCODE_PUSH_TRUE:
                break;
            case VM_OPCODE_PUSH_FALSE:
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
            case VM_OPCODE_PUSH_STRING:
                break;
            case VM_OPCODE_PUSH_GLOBAL_BY_NAME: {
                const auto name = utils::readString(text, thread->instructionPos, data);
                if (auto slot = _globalManager.resolveOffset(name)) {
                    _moduleManager.changeNameToSlot(
                        thread->script, thread->instructionPos - 5, VM_OPCODE_PUSH_GLOBAL, slot.value());
                    thread->stack[thread->stackPos++] = _globalManager.global(slot.value());
                } else {
                    // TODO: log
                }
                break;
            }
            case VM_OPCODE_PUSH_GLOBAL:
                break;
            case VM_OPCODE_PUSH_LOCAL:
                break;
            case VM_OPCODE_POP_GLOBAL_BY_NAME: {
                const auto name = utils::readString(text, thread->instructionPos, data);
                if (auto slot = _globalManager.resolveOffset(name)) {
                    _moduleManager.changeNameToSlot(
                        thread->script, thread->instructionPos - 5, VM_OPCODE_POP_GLOBAL, slot.value());
                    _globalManager.global(slot.value()) = thread->stack[--thread->stackPos];
                } else {
                    // TODO: log
                }
                break;
            }
            case VM_OPCODE_POP_GLOBAL:
                break;
            case VM_OPCODE_POP_LOCAL:
                break;
            case VM_OPCODE_POP:
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
            case VM_OPCODE_CALL_NATIVE:
                break;
            case VM_OPCODE_CALL_BY_NAME:
                break;
            case VM_OPCODE_CALL:
                break;
            case VM_OPCODE_RETURN:
                break;
            case VM_OPCODE_JUMP:
                break;
            case VM_OPCODE_JUMP_IF_FALSE:
                break;
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

    ModuleManager _moduleManager;
    GlobalManager _globalManager;
    Cache _cache;
    utils::Pool<ScriptThread> _threads;
    std::vector<ScriptThreadHandle> _threadHandles;
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

vm_script_h vm_load_script(vm_t vm, vm_script_h handle) {
    return vm ? vm->as<vm::VM>()->loadScript(handle, false) : vm_script_h{};
}

vm_script_h vm_load_script_from_file(vm_t vm, vm_utf8_t file) {
    return vm ? vm->as<vm::VM>()->loadScriptFromFile(file, true) : vm_script_h{};
}

vm_script_h vm_load_script_from_memory(vm_t vm, const vm_char_t* code, vm_uint32_t code_size) {
    return vm ? vm->as<vm::VM>()->loadScriptFromMemory({code, code_size}, true) : vm_script_h{};
}

void vm_tick(vm_t vm) {
    if (vm) {
        vm->as<vm::VM>()->tick();
    }
}
