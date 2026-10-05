#ifndef VM_GLOBAL_MANAGER_HPP
#define VM_GLOBAL_MANAGER_HPP

#include "vm.hpp"

namespace vm {

class GlobalManager {
public:
    GlobalManager();

    void addGlobals(std::span<const uint8_t> bytecode);

    [[nodiscard]] std::optional<vm_uint32_t> resolveOffset(std::string_view name) const noexcept;

    [[nodiscard]] vm_value_t& global(vm_uint32_t offset, size_t i = 0) noexcept;

private:
    void addGlobal(std::string_view name, vm_value_type_t type);

    utils::StringPool _names;
    std::vector<vm_uint32_t> _offsets;
    std::vector<vm_value_t> _globals;
};

} // namespace vm

#endif // VM_GLOBAL_MANAGER_HPP
