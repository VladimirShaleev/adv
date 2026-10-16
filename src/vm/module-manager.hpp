#ifndef VM_MODULE_MANAGER_HPP
#define VM_MODULE_MANAGER_HPP

#include "cache.hpp"

namespace vm {

class Module {
public:
    explicit Module(std::span<const uint8_t> bytecode);

    [[nodiscard]] std::span<const uint8_t> text() const noexcept { return _text; };
    [[nodiscard]] std::span<const uint8_t> data() const noexcept { return _data; };
    [[nodiscard]] std::span<const uint8_t> bytecode() const noexcept { return _bytecode; };

    [[nodiscard]] std::string_view name() const noexcept;
    [[nodiscard]] vm_uint32_t checksum() const noexcept;

    void modifyOpcode(vm_uint32_t instructionPos, vm_opcode_t newOpcode, vm_uint32_t slot) noexcept;

private:
    struct Change {
        vm_uint32_t offset;
        vm_opcode_t opcode;
        vm_uint32_t value;
    };

    std::span<uint8_t> _text{};
    std::span<const uint8_t> _data{};
    std::vector<uint8_t> _bytecode;
    std::vector<Change> _changes;
};

class ModuleManager {
public:
    explicit ModuleManager(Cache& cache) noexcept;

    [[nodiscard]] bool hasModule(std::string_view moduleName) const noexcept;
    [[nodiscard]] Module* getModule(vm_script_h script) noexcept;
    [[nodiscard]] const Module* getModule(vm_script_h script) const noexcept;

#if !defined(__EMSCRIPTEN__)
    [[nodiscard]] std::pair<vm_script_h, bool> loadScript(vm_utf8_t file, std::span<const std::filesystem::path> dirs);
#endif
    [[nodiscard]] std::pair<vm_script_h, bool>
    loadScript(std::span<const vm_char_t> code, std::string_view moduleName = {});

private:
    [[nodiscard]] static std::string_view getModuleName(std::span<const vm_uint8_t> bytecode) noexcept;
    [[nodiscard]] static std::span<const vm_char_t>
    tryTranslate(std::span<const vm_char_t> code, std::vector<vm_uint8_t>& buffer) noexcept;

    Cache& _cache;
    utils::FlatMap<vm_string_h, vm_script_h, Module> _modules;
};

} // namespace vm

#endif // VM_MODULE_MANAGER_HPP
