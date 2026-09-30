#include "script.hpp"

struct _vm : public utils::Object {};

namespace vm {

enum class Format {
    Undefined,
    Binary,
    Text
};

struct Module {
    using Handle = vm_script_h;

    vm_script_header_t header;
    vm_script_table_t table;
    std::vector<uint8_t> bytecode;
};

class VM final : public _vm {
public:
    VM() : _mapModules(16, 2048, 64), _modules(16), _threads(64), _mapGlobals(512, 4096, 2048) {}

    void loadScript(vm_script_h handle) noexcept {}

    [[nodiscard]] vm_script_h loadScriptFromMemory(std::span<const vm_char_t> code) noexcept {
        std::vector<vm_uint8_t> tmpBuffer;
        if (!utils::isBinaryCode(code)) {
            if (code = tryTranslate(code, tmpBuffer); code.empty()) {
                return UndefinedScript;
            }
        }
        return {};
    }

    [[nodiscard]] vm_script_h loadScriptFromFile(vm_utf8_t file) noexcept {
        std::ifstream stream(file, std::ios::binary);
        if (!stream.is_open()) {
            return UndefinedScript;
        }
        auto size = stream.tellg();
        stream.seekg(0, std::ios::end);
        size = stream.tellg() - size;
        stream.seekg(0, std::ios::beg);

        std::vector<vm_char_t> bytes;
        bytes.resize(size);
        stream.read(bytes.data(), size);
        stream.close();

        return loadScriptFromMemory({bytes.data(), bytes.size()});
    }

private:
    std::span<const vm_char_t> tryTranslate(std::span<const vm_char_t> code, std::vector<vm_uint8_t>& buffer) {
        utils::TranslateResult error{};
        buffer = utils::translateTextToBinary(code, error);
        if (error.errorCode == utils::TranslateResult::NoError) {
            return {(const vm_char_t*)buffer.data(), buffer.size()};
        }
        return {};
    }

    utils::StringPool _mapModules;
    utils::Pool<Module> _modules;
    utils::Pool<ScriptThread> _threads;
    std::vector<ScriptThreadHandle> _threadHandles;
    utils::StringPool _mapGlobals;
    std::vector<vm_value_t> _globals;
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
