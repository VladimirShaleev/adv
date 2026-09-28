#include "vm.hpp"

struct _vm : public utils::Object {};

namespace vm {

class VM final : public _vm {
public:
    void loadScript(vm_script_h handle) noexcept {}

    vm_script_h loadScriptFromMemory(std::span<const uint8_t> code) noexcept { return {}; }

    vm_script_h loadScriptFromFile(vm_utf8_t file) noexcept { return {}; }
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

vm_script_h vm_load_script_from_memory(vm_t vm, const vm_uint8_t* code, vm_uint32_t code_size) {
    return vm ? vm->as<vm::VM>()->loadScriptFromMemory({code, code_size}) : vm_script_h{};
}
