#include "global-manager.hpp"
#include "module-manager.hpp"
#include "script.hpp"

struct _vm : public utils::Object {};

namespace vm {

class VM final : public _vm {
public:
    VM()
    /* _threads(64), { _modules.emplace_back(); } */
    {}

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
        while (thread->state == ThreadState::Running && thread->instructionPos < text.size()) {
            const auto opcode = text[thread->instructionPos];
            switch (opcode) {
            case VM_OPCODE_NO_OPERATION: {
                const auto vaule = utils::readInt32(text, thread->instructionPos);
                break;
            }
            case VM_OPCODE_PUSH_TRUE:
                break;
            case VM_OPCODE_PUSH_FALSE:
                break;
            case VM_OPCODE_PUSH_INT:
                break;
            case VM_OPCODE_PUSH_FLOAT:
                break;
            case VM_OPCODE_PUSH_STRING:
                break;
            case VM_OPCODE_PUSH_GLOBAL_BY_NAME:
                break;
            case VM_OPCODE_PUSH_GLOBAL:
                break;
            case VM_OPCODE_PUSH_LOCAL:
                break;
            case VM_OPCODE_POP_GLOBAL_BY_NAME:
                break;
            case VM_OPCODE_POP_GLOBAL:
                break;
            case VM_OPCODE_POP_LOCAL:
                break;
            case VM_OPCODE_POP:
                break;
            case VM_OPCODE_BOOL_TO_INT:
                break;
            case VM_OPCODE_BOOL_TO_FLOAT:
                break;
            case VM_OPCODE_BOOL_TO_STRING:
                break;
            case VM_OPCODE_INT_TO_BOOL:
                break;
            case VM_OPCODE_INT_TO_FLOAT:
                break;
            case VM_OPCODE_INT_TO_STRING:
                break;
            case VM_OPCODE_FLOAT_TO_BOOL:
                break;
            case VM_OPCODE_FLOAT_TO_INT:
                break;
            case VM_OPCODE_FLOAT_TO_STRING:
                break;
            case VM_OPCODE_STRING_TO_BOOL:
                break;
            case VM_OPCODE_STRING_TO_INT:
                break;
            case VM_OPCODE_STRING_TO_FLOAT:
                break;
            case VM_OPCODE_BITWISE_NOT:
                break;
            case VM_OPCODE_LOGICAL_NOT:
                break;
            case VM_OPCODE_NUMERIC_NEGATION:
                break;
            case VM_OPCODE_BITWISE_AND:
                break;
            case VM_OPCODE_BITWISE_OR:
                break;
            case VM_OPCODE_BITWISE_XOR:
                break;
            case VM_OPCODE_LOGIC_SHIFT_LEFT:
                break;
            case VM_OPCODE_LOGIC_SHIFT_RIGHT:
                break;
            case VM_OPCODE_ARITHMETIC_SHIFT_LEFT:
                break;
            case VM_OPCODE_ARITHMETIC_SHIFT_RIGHT:
                break;
            case VM_OPCODE_ADD:
                break;
            case VM_OPCODE_SUBTRACT:
                break;
            case VM_OPCODE_MULTIPLY:
                break;
            case VM_OPCODE_DIVISION:
                break;
            case VM_OPCODE_MOD:
                break;
            case VM_OPCODE_EQUAL:
                break;
            case VM_OPCODE_NOT_EQUAL:
                break;
            case VM_OPCODE_LESS:
                break;
            case VM_OPCODE_LESS_EQUAL:
                break;
            case VM_OPCODE_GREAT:
                break;
            case VM_OPCODE_GREAT_EQUAL:
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

    ModuleManager _moduleManager;
    GlobalManager _globalManager;
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
