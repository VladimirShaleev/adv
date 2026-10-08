#ifndef VM_GLOBAL_MANAGER_HPP
#define VM_GLOBAL_MANAGER_HPP

#include "vm.hpp"

namespace vm {

class GlobalManager {
public:
    struct Func {
        vm_script_h script;
        vm_uint32_t offset;
        vm_uint16_t paramCount;
        vm_uint16_t frameSlotCount;
        std::optional<vm_value_type_t> returnType;
    };

    GlobalManager();

    void addGlobals(vm_script_h script, std::span<const uint8_t> bytecode);

    [[nodiscard]] std::optional<vm_uint32_t> resolveVarSlot(std::string_view name) const noexcept;
    [[nodiscard]] std::optional<vm_uint32_t> resolveFunc(std::string_view name) const noexcept;

    [[nodiscard]] vm_value_t& globalVar(vm_uint32_t offset, size_t i = 0) noexcept;
    [[nodiscard]] const Func& globalFun(vm_uint32_t offset) const noexcept;

private:
    void addGlobalVar(std::string_view name, vm_value_type_t type);
    void addGlobalFun(
        std::string_view name,
        vm_uint16_t paramCount,
        vm_uint16_t frameSlotCount,
        vm_script_h script,
        vm_uint32_t offset,
        std::optional<vm_value_type_t> returnType);

    utils::StringPool _varNames;
    utils::StringPool _funNames;
    std::vector<vm_uint32_t> _varOffsets;
    std::vector<vm_value_t> _vars;
    std::vector<Func> _funcs;
};

} // namespace vm

#endif // VM_GLOBAL_MANAGER_HPP
