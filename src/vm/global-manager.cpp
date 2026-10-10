#include "global-manager.hpp"

namespace vm {

GlobalManager::GlobalManager(Cache& cache) noexcept : _cache(cache) {}

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

GlobalManager::VarSlot GlobalManager::resolveVar(std::string_view name) const noexcept {
    if (auto str = _cache.pool.find(name)) {
        const auto [slot, _] = _vars.find(str.value());
        return slot;
    }
    return Undefined;
}

GlobalManager::FuncSlot GlobalManager::resolveFunc(std::string_view name) const noexcept {
    if (auto str = _cache.pool.find(name)) {
        const auto [slot, _] = _funcs.find(str.value());
        return slot;
    }
    return Undefined;
}

void GlobalManager::setVar(VarSlot slot, const vm_value_t& value) noexcept { *_vars.get(slot) = value; }

const vm_value_t& GlobalManager::getVar(VarSlot slot) const noexcept { return *_vars.get(slot); }

const GlobalManager::Func& GlobalManager::getFunc(FuncSlot slot) const noexcept { return *_funcs.get(slot); }

void GlobalManager::addGlobalVar(std::string_view name, vm_value_type_t type) {
    if (auto slot = resolveVar(name); slot != Undefined) {
        if (getVar(slot).type != type) {
            // TODO: add log
        }
    }
    const auto str = _cache.pool.insert(name);
    const auto _ = _vars.emplace(str, type);
}

void GlobalManager::addGlobalFun(
    std::string_view name,
    vm_uint16_t paramCount,
    vm_uint16_t frameSlotCount,
    vm_script_h script,
    vm_uint32_t offset,
    std::optional<vm_value_type_t> returnType) {
    const auto str = _cache.pool.insert(name);
    const auto _ = _funcs.emplace(str, script, offset, paramCount, frameSlotCount, returnType);
}

} // namespace vm
