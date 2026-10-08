#include "global-manager.hpp"

namespace vm {

GlobalManager::GlobalManager() : _varNames(256, 2048, 1024), _funNames(256, 2048, 1024) {}

void GlobalManager::addGlobals(vm_script_h script, std::span<const uint8_t> bytecode) {
    const auto data = utils::read<utils::SectionData>(bytecode).data;
    const auto exports = utils::read<vm_script_export_t>(bytecode);
    const auto moduleName = utils::readString(exports.module_name, data);
    const size_t varCount = exports.var_count;
    const size_t funCount = exports.func_count;
    for (size_t i = 0; i < varCount; ++i) {
        const auto var = utils::read<vm_script_export_var_t>(bytecode, i);
        auto name = utils::readString(var.name, data);
        auto type = static_cast<vm_value_type_t>(var.type);
        addGlobalVar(name, type);
    }
    for (size_t i = 0; i < funCount; ++i) {
        const auto fun = utils::read<vm_script_export_func_t>(bytecode, i);
        auto name = utils::readString(fun.name, data);
        std::optional<vm_value_type_t> returnType = std::nullopt;
        if (fun.return_type >= VM_VALUE_TYPE_BOOL && fun.return_type <= VM_VALUE_TYPE_STRING) {
            returnType = static_cast<vm_value_type_t>(fun.return_type);
        }
        addGlobalFun(name, fun.param_count, fun.frame_slot_count, script, fun.text_offset, returnType);
    }
}

std::optional<vm_uint32_t> GlobalManager::resolveVarSlot(std::string_view name) const noexcept {
    if (auto str = _varNames.find(name)) {
        return _varOffsets[str.value().handle - 1];
    }
    return std::nullopt;
}

std::optional<vm_uint32_t> GlobalManager::resolveFunc(std::string_view name) const noexcept {
    if (auto str = _funNames.find(name)) {
        return str.value().handle - 1;
    }
    return std::nullopt;
}

vm_value_t& GlobalManager::globalVar(vm_uint32_t offset, size_t i) noexcept { return _vars[offset + i]; }

const GlobalManager::Func& GlobalManager::globalFun(vm_uint32_t offset) const noexcept { return _funcs[offset]; }

void GlobalManager::addGlobalVar(std::string_view name, vm_value_type_t type) {
    if (auto slot = resolveVarSlot(name)) {
        if (_vars[slot.value()].type != type) {
            // TODO: add log
        }
        return;
    }
    const auto str = _varNames.insert(name);
    const auto offset = static_cast<vm_uint32_t>(_vars.size());
    _varOffsets.emplace_back(offset);
    _vars.emplace_back(type);
    assert(_varOffsets.size() == str.handle);
}

void GlobalManager::addGlobalFun(
    std::string_view name,
    vm_uint16_t paramCount,
    vm_uint16_t frameSlotCount,
    vm_script_h script,
    vm_uint32_t offset,
    std::optional<vm_value_type_t> returnType) {
    const auto str = _funNames.insert(name);
    const auto index = static_cast<size_t>(str.handle - 1);
    if (index >= _funcs.size()) {
        _funcs.emplace_back(script, offset, paramCount, frameSlotCount, returnType);
    } else {
        _funcs[index].script = script;
        _funcs[index].offset = offset;
        _funcs[index].paramCount = paramCount;
        _funcs[index].frameSlotCount = frameSlotCount;
        _funcs[index].returnType = returnType;
    }
}

} // namespace vm
