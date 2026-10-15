#ifndef VM_GLOBAL_MANAGER_HPP
#define VM_GLOBAL_MANAGER_HPP

#include "cache.hpp"

namespace vm {

class GlobalManager {
public:
    struct VarSlot {
        vm_uint32_t handle;
    };

    struct FuncSlot {
        vm_uint32_t handle;
    };

    struct Func {
        vm_script_h script;
        vm_uint32_t offset;
        vm_uint16_t paramCount;
        vm_uint16_t frameSlotCount;
        std::optional<vm_value_type_t> returnType;
    };

    explicit GlobalManager(Cache& cache) noexcept;

    void addGlobals(vm_script_h script, std::span<const uint8_t> bytecode);

    [[nodiscard]] VarSlot resolveVar(std::string_view name) const noexcept;
    [[nodiscard]] FuncSlot resolveFunc(std::string_view name) const noexcept;

    void setVar(VarSlot slot, const vm_value_t& value) noexcept;
    [[nodiscard]] const vm_value_t& getVar(VarSlot slot) const noexcept;
    [[nodiscard]] const Func& getFunc(FuncSlot slot) const noexcept;

private:
    void addGlobalVar(std::string_view name, vm_value_type_t type);
    void addGlobalFun(
        std::string_view name,
        vm_uint16_t paramCount,
        vm_uint16_t frameSlotCount,
        vm_script_h script,
        vm_uint32_t offset,
        std::optional<vm_value_type_t> returnType);

    Cache& _cache;
    utils::FlatMap<vm_string_h, VarSlot, vm_value_t> _vars;
    utils::FlatMap<vm_string_h, FuncSlot, Func> _funcs;
};

} // namespace vm

#endif // VM_GLOBAL_MANAGER_HPP
