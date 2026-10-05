#include "global-manager.hpp"

namespace vm {

GlobalManager::GlobalManager() : _names(512, 4096, 2048) {}

void GlobalManager::addGlobals(std::span<const uint8_t> bytecode) {
    const auto data = utils::read<utils::SectionData>(bytecode).data;
    const size_t varCount = utils::read<vm_script_export_t>(bytecode).var_count;
    for (size_t i = 0; i < varCount; ++i) {
        const auto var = utils::read<vm_script_export_var_t>(bytecode, i);
        auto name = utils::readString(var.name, data);
        auto type = static_cast<vm_value_type_t>(var.type);
        addGlobal(name, type);
    }
}

std::optional<vm_uint32_t> GlobalManager::resolveOffset(std::string_view name) const noexcept {
    if (auto str = _names.find(name)) {
        return _offsets[str.value().handle - 1];
    }
    return std::nullopt;
}

vm_value_t& GlobalManager::global(vm_uint32_t offset, size_t i) noexcept { return _globals[offset + i]; }

void GlobalManager::addGlobal(std::string_view name, vm_value_type_t type) {
    if (_names.find(name)) {
        // TODO: add log
        return;
    }
    const auto str = _names.insert(name);
    const auto offset = static_cast<vm_uint32_t>(_globals.size());
    _offsets.emplace_back(offset);
    _globals.emplace_back(type);
}

} // namespace vm
