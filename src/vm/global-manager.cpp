#include "global-manager.hpp"

namespace vm {

GlobalManager::GlobalManager(Cache& cache) noexcept : _cache(cache) {}

void GlobalManager::addGlobals(vm_script_h script, std::span<const uint8_t> bytecode) {
    const auto data = utils::read<utils::SectionData>(bytecode).data;
    const auto exports = utils::read<vm_script_export_t>(bytecode);
    for (size_t i = 0; i < exports.struct_count; ++i) {
        const auto structure = utils::read<vm_script_export_struct_t>(bytecode, i);
        auto nn = utils::readString(structure.name, data);
        const auto structureName = _cache.pool.insert(nn);
        auto& layout = _structures[structureName];
        layout.fields.clear();
        layout.fields.reserve(structure.field_count);
        for (size_t fieldIndex = 0; fieldIndex < structure.field_count; ++fieldIndex) {
            const auto field = utils::read<vm_script_export_field_t>(bytecode, i, fieldIndex);
            layout.fields.push_back({
                _cache.pool.insert(utils::readString(field.name, data)),
                readType(field.type, data)});
        }
    }

    const size_t varCount = exports.var_count;
    const size_t funCount = exports.func_count;
    for (size_t i = 0; i < varCount; ++i) {
        const auto var = utils::read<vm_script_export_var_t>(bytecode, i);
        const auto name = _cache.pool.insert(utils::readString(var.name, data));
        _pendingVars.insert_or_assign(name, PendingVar{readType(var.type, data)});
    }

    for (size_t i = 0; i < funCount; ++i) {
        const auto fun = utils::read<vm_script_export_func_t>(bytecode, i);
        auto name = utils::readString(fun.name, data);
        std::optional<vm_value_type_t> returnType = std::nullopt;
        if (fun.return_type >= VM_VALUE_TYPE_BOOL && fun.return_type <= VM_VALUE_TYPE_STRING) {
            returnType = static_cast<vm_value_type_t>(fun.return_type);
        }
        addGlobalFun(name, fun.param_count, script, fun.text_offset, returnType);
    }
}

GlobalManager::VarSlot GlobalManager::resolveVar(std::string_view name) {
    if (auto str = _cache.pool.find(name)) {
        const auto [slot, var] = _vars.find(str.value());
        if (var) {
            return slot;
        }
    }

    auto rootName = name;
    while (true) {
        if (auto root = _cache.pool.find(rootName)) {
            if (auto var = _pendingVars.find(root.value()); var != _pendingVars.end()) {
                if (!var->second.materialized && !materializeGlobalVar(rootName, var->second)) {
                    return Undefined;
                }
                if (auto str = _cache.pool.find(name)) {
                    return _vars.find(str.value()).first;
                }
                return Undefined;
            }
        }
        const auto separator = rootName.rfind('.');
        if (separator == std::string_view::npos) {
            break;
        }
        rootName = rootName.substr(0, separator);
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

GlobalManager::TypeRef GlobalManager::readType(vm_uint32_t packedType, std::span<const uint8_t> data) {
    const auto baseType = utils::exportTypeBase(packedType);
    if (baseType == utils::ExportTypeStructure) {
        const auto nameOffset = utils::exportTypeDataOffset(packedType);
        return {baseType, _cache.pool.insert(utils::readString(nameOffset, data))};
    }
    return {baseType, {}};
}

bool GlobalManager::collectGlobalSlots(
    std::string& name,
    const TypeRef& type,
    std::vector<std::pair<std::string, vm_value_type_t>>& slots,
    std::vector<vm_string_h>& structureStack) const {
    if (type.baseType >= VM_VALUE_TYPE_BOOL && type.baseType <= VM_VALUE_TYPE_STRING) {
        slots.emplace_back(name, static_cast<vm_value_type_t>(type.baseType));
        return true;
    }
    if (type.baseType != utils::ExportTypeStructure ||
        std::find(structureStack.begin(), structureStack.end(), type.structureName) != structureStack.end()) {
        return false;
    }
    const auto structure = _structures.find(type.structureName);
    if (structure == _structures.end()) {
        return false;
    }

    structureStack.push_back(type.structureName);
    const auto baseNameLength = name.length();
    for (const auto& field : structure->second.fields) {
        name.resize(baseNameLength);
        name.push_back('.');
        name += _cache.pool.get(field.name);
        if (!collectGlobalSlots(name, field.type, slots, structureStack)) {
            structureStack.pop_back();
            name.resize(baseNameLength);
            return false;
        }
    }
    structureStack.pop_back();
    name.resize(baseNameLength);
    return true;
}

bool GlobalManager::materializeGlobalVar(std::string_view name, PendingVar& var) {
    std::vector<std::pair<std::string, vm_value_type_t>> slots;
    std::vector<vm_string_h> structureStack;
    std::string slotName{name};
    if (!collectGlobalSlots(slotName, var.type, slots, structureStack)) {
        return false;
    }
    for (const auto& [slotName, type] : slots) {
        const auto key = _cache.pool.insert(slotName);
        const auto result = _vars.emplace(key, type);
        assert(result.first != Undefined);
    }
    var.materialized = true;
    return true;
}

void GlobalManager::addGlobalFun(
    std::string_view name,
    vm_uint16_t paramCount,
    vm_script_h script,
    vm_uint32_t offset,
    std::optional<vm_value_type_t> returnType) {
    const auto str = _cache.pool.insert(name);
    const auto _ = _funcs.emplace(str, script, offset, paramCount, returnType);
}

} // namespace vm
