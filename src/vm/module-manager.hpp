#ifndef VM_MODULE_MANAGER_HPP
#define VM_MODULE_MANAGER_HPP

#include "vm.hpp"

namespace vm {

class ModuleManager {
public:
    ModuleManager();

    [[nodiscard]] std::span<const uint8_t> getModule(vm_script_h script) const noexcept;

    [[nodiscard]] vm_script_h loadScript(vm_utf8_t file);
    [[nodiscard]] vm_script_h loadScript(std::span<const vm_char_t> code, std::string_view moduleName = {});

private:
    [[nodiscard]] static std::string_view getModuleName(std::span<const vm_uint8_t> bytecode) noexcept;
    [[nodiscard]] static std::span<const vm_char_t>
    tryTranslate(std::span<const vm_char_t> code, std::vector<vm_uint8_t>& buffer) noexcept;

    utils::StringPool _mapModules;
    std::vector<std::vector<uint8_t>> _modules;
};

} // namespace vm

#endif // VM_MODULE_MANAGER_HPP
