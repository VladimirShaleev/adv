#include "global-manager.hpp"
#include "module-manager.hpp"
#include "script.hpp"

struct _vm : public utils::Object {};

namespace vm {

enum class Format {
    Undefined,
    Binary,
    Text
};

class VM final : public _vm {
public:
    VM()
    /* _threads(64), { _modules.emplace_back(); } */
    {}

    void loadScript(vm_script_h handle) noexcept {
        auto module = _moduleManager.getModule(handle);
        _globalManager.addGlobals(module);
    }

    [[nodiscard]] vm_script_h loadScriptFromMemory(std::span<const vm_char_t> code) noexcept {
        auto script = _moduleManager.loadScript(code);
        loadScript(script);
        return script;
    }

    [[nodiscard]] vm_script_h loadScriptFromFile(vm_utf8_t file) noexcept {
        auto script = _moduleManager.loadScript(file);
        loadScript(script);
        return script;
    }

private:
    ModuleManager _moduleManager;
    GlobalManager _globalManager;
    // utils::Pool<ScriptThread> _threads;
    // std::vector<ScriptThreadHandle> _threadHandles;
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

vm_uint32_t vm_adler_32(const vm_char_t* data, vm_uint32_t size) {
    std::span<const vm_char_t> binary{data, size};
    auto view = std::views::all(binary);
    return utils::adler32(std::move(view));
}

vm_uint16_t vm_read_uint16(const vm_char_t* buffer, vm_uint32_t* offset) {
    if (buffer && offset) {
        auto resultOffset = static_cast<size_t>(*offset);
        const auto result = utils::readUint16((const vm_uint8_t*)buffer, resultOffset);
        *offset = static_cast<vm_uint32_t>(resultOffset);
        return result;
    }
    return 0u;
}

vm_uint32_t vm_read_uint32(const vm_char_t* buffer, vm_uint32_t* offset) {
    if (buffer && offset) {
        auto resultOffset = static_cast<size_t>(*offset);
        const auto result = utils::readUint32((const vm_uint8_t*)buffer, resultOffset);
        *offset = static_cast<vm_uint32_t>(resultOffset);
        return result;
    }
    return 0u;
}

vm_sint32_t vm_read_int32(const vm_char_t* buffer, vm_uint32_t* offset) {
    if (buffer && offset) {
        auto resultOffset = static_cast<size_t>(*offset);
        const auto result = utils::readInt32((const vm_uint8_t*)buffer, resultOffset);
        *offset = static_cast<vm_uint32_t>(resultOffset);
        return result;
    }
    return 0u;
}

vm_float32_t vm_read_float(const vm_char_t* buffer, vm_uint32_t* offset) {
    if (buffer && offset) {
        auto resultOffset = static_cast<size_t>(*offset);
        const auto result = utils::readFloat((const vm_uint8_t*)buffer, resultOffset);
        *offset = static_cast<vm_uint32_t>(resultOffset);
        return result;
    }
    return 0u;
}

vm_bool_t vm_validate_checksum(const vm_char_t* data, vm_uint32_t size) {
    std::span<const vm_char_t> binary{data, size};
    return utils::validateChecksum(binary) ? 1 : 0;
}

vm_bool_t vm_validate_version(const vm_char_t* data, vm_uint32_t size) {
    std::span<const vm_char_t> binary{data, size};
    return utils::validateVersion(binary, VM_VERSION) ? 1 : 0;
}

vm_uint16_t vm_swap_uint16(vm_uint16_t value) { return utils::swapBytes(value); }

vm_uint32_t vm_swap_uint32(vm_uint32_t value) { return utils::swapBytes(value); }

vm_float32_t vm_swap_float32(vm_float32_t value) {
    vm_uint32_t bits;
    std::memcpy(&bits, &value, sizeof(vm_float32_t));
    bits = utils::swapBytes(bits);
    std::memcpy(&value, &bits, sizeof(vm_float32_t));
    return value;
}

vm_script_header_t vm_swap_script_header(const vm_script_header_t* header) {
    if (!header) {
        return {};
    }
    return std::endian::native == std::endian::little ? *header : utils::swapBytes(*header);
}

vm_script_table_t vm_swap_script_table(const vm_script_table_t* table) {
    if (!table) {
        return {};
    }
    return std::endian::native == std::endian::little ? *table : utils::swapBytes(*table);
}

vm_script_export_t vm_swap_script_export(const vm_script_export_t* export_table) {
    if (!export_table) {
        return {};
    }
    return std::endian::native == std::endian::little ? *export_table : utils::swapBytes(*export_table);
}

vm_script_export_var_t vm_swap_script_export_var(const vm_script_export_var_t* export_var) {
    if (!export_var) {
        return {};
    }
    return std::endian::native == std::endian::little ? *export_var : utils::swapBytes(*export_var);
}

vm_script_export_func_t vm_swap_script_export_func(const vm_script_export_func_t* export_func) {
    if (!export_func) {
        return {};
    }
    return std::endian::native == std::endian::little ? *export_func : utils::swapBytes(*export_func);
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
    if (vm) {
        vm->as<vm::VM>()->loadScript(handle);
    }
    return handle;
}

vm_script_h vm_load_script_from_file(vm_t vm, vm_utf8_t file) {
    return vm ? vm->as<vm::VM>()->loadScriptFromFile(file) : vm_script_h{};
}

vm_script_h vm_load_script_from_memory(vm_t vm, const vm_char_t* code, vm_uint32_t code_size) {
    return vm ? vm->as<vm::VM>()->loadScriptFromMemory({code, code_size}) : vm_script_h{};
}
