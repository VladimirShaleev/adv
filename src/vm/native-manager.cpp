#include "native-manager.hpp"

namespace vm {

NativeManager::NativeManager(Cache& cache) noexcept : _cache(cache) {}

void NativeManager::addNatives(vm_script_h script, std::span<const uint8_t> bytecode) {
    const auto data = utils::read<utils::SectionData>(bytecode).data;
    const auto exports = utils::read<vm_script_export_t>(bytecode);
    std::unordered_map<std::string, size_t> structuresByName;
    structuresByName.reserve(exports.struct_count);
    for (size_t i = 0; i < exports.struct_count; ++i) {
        const auto structure = utils::read<vm_script_export_struct_t>(bytecode, i);
        structuresByName.emplace(std::string(utils::readString(structure.name, data)), i);
    }

    const auto countSlots = [&](auto&& self, vm_uint32_t packedType, std::vector<size_t>& structureStack) -> vm_uint32_t {
        const auto baseType = utils::exportTypeBase(packedType);
        if (baseType == VM_VALUE_TYPE_UNKNOWN) {
            return 0;
        }
        if (baseType >= VM_VALUE_TYPE_BOOL && baseType <= VM_VALUE_TYPE_STRING) {
            return 1;
        }
        if (baseType != utils::ExportTypeStructure) {
            return 0;
        }

        const auto structureName = utils::readString(utils::exportTypeDataOffset(packedType), data);
        const auto it = structuresByName.find(std::string(structureName));
        if (it == structuresByName.end() ||
            std::find(structureStack.begin(), structureStack.end(), it->second) != structureStack.end()) {
            return 1;
        }

        structureStack.push_back(it->second);
        const auto structure = utils::read<vm_script_export_struct_t>(bytecode, it->second);
        vm_uint32_t slots = 0;
        for (size_t i = 0; i < structure.field_count; ++i) {
            const auto field = utils::read<vm_script_export_field_t>(bytecode, it->second, i);
            slots += self(self, field.type, structureStack);
        }
        structureStack.pop_back();
        return slots;
    };

    const size_t nativeCount = exports.native_count;
    for (size_t i = 0; i < nativeCount; ++i) {
        const auto native = utils::read<vm_script_export_native_t>(bytecode, i);
        auto name = utils::readString(native.name, data);
        vm_uint32_t argSlotCount = 0;
        std::vector<size_t> structureStack;
        for (size_t j = 0; j < native.param_count; ++j) {
            const auto param = utils::read<utils::NativeParam>(bytecode, i, j).param;
            argSlotCount += countSlots(countSlots, param.type, structureStack);
        }
        const auto returnCount =
            static_cast<vm_uint16_t>(countSlots(countSlots, native.return_type, structureStack));
        addNative(script, name, native.param_count, argSlotCount, returnCount);
    }
}

const Native* NativeManager::func(vm_native_h native) const noexcept { return _natives.get(native); }

vm_native_h NativeManager::find(std::string_view name) const noexcept {
    if (auto str = _cache.pool.find(name)) {
        auto [handle, _] = _natives.find(str.value());
        return handle;
    }
    return Undefined;
}

vm_native_h NativeManager::registerFunc(std::string_view name, vm_native_callback_t func, vm_data_t data) noexcept {
    auto str = _cache.pool.insert(name);
    auto [handle, native] = _natives.find(str);
    if (native) {
        native->func = func;
        native->data = data;
        return handle;
    } else {
        auto [resultHandle, _] = _natives.emplace(str, str, vm_script_h{0}, func, 0, 0, 0, data);
        return resultHandle;
    }
    return Undefined;
}

vm_script_h NativeManager::script(vm_native_h native) const noexcept {
    if (const auto nativePtr = _natives.get(native)) {
        return nativePtr->script;
    }
    return Undefined;
}

const vm_reflect_func_t* NativeManager::reflect(vm_native_h native, std::span<const uint8_t> bytecode) {
    const auto nativePtr = _natives.get(native);
    if (!nativePtr) {
        return nullptr;
    }
    if (auto it = _reflections.find(native); it != _reflections.end()) {
        return it->second.get();
    }
    auto targetName = _cache.pool.get(nativePtr->name);
    const auto data = utils::read<utils::SectionData>(bytecode).data;
    const auto exports = utils::read<vm_script_export_t>(bytecode);
    const size_t nativeCount = exports.native_count;
    for (size_t i = 0; i < nativeCount; ++i) {
        const auto nativeData = utils::read<vm_script_export_native_t>(bytecode, i);
        auto name = utils::readString(nativeData.name, data);
        if (name == targetName) {
            auto func = std::make_unique<FuncReflect>();
            func->nameFunc = name;

            func->structures.reserve(exports.struct_count);
            for (size_t j = 0; j < exports.struct_count; ++j) {
                const auto structureData = utils::read<vm_script_export_struct_t>(bytecode, j);
                auto& structure = func->structures.emplace_back();
                structure.name = utils::readString(structureData.name, data);
                structure.fieldNames.reserve(structureData.field_count);
                structure.fields.resize(structureData.field_count);
                structure.fieldTypes.reserve(structureData.field_count);
                for (size_t k = 0; k < structureData.field_count; ++k) {
                    const auto field = utils::read<vm_script_export_field_t>(bytecode, j, k);
                    structure.fieldNames.emplace_back(utils::readString(field.name, data));
                    structure.fieldTypes.emplace_back(field.type);
                }
            }

            std::unordered_map<std::string, const vm_reflect_struct_t*> structsByName;
            structsByName.reserve(func->structures.size());
            for (const auto& structure : func->structures) {
                structsByName.emplace(structure.name, &structure.info);
            }

            const auto countSlots =
                [&](auto&& self, vm_uint32_t packedType, std::vector<size_t>& structureStack) -> vm_uint32_t {
                const auto baseType = utils::exportTypeBase(packedType);
                if (baseType == VM_VALUE_TYPE_UNKNOWN) {
                    return 0;
                }
                if (baseType >= VM_VALUE_TYPE_BOOL && baseType <= VM_VALUE_TYPE_STRING) {
                    return 1;
                }
                if (baseType != utils::ExportTypeStructure) {
                    return 0;
                }

                const auto structureName = utils::readString(utils::exportTypeDataOffset(packedType), data);
                const auto it = std::find_if(
                    func->structures.begin(),
                    func->structures.end(),
                    [&](const auto& structure) { return structure.name == structureName; });
                if (it == func->structures.end()) {
                    return 1;
                }
                const auto structureIndex = static_cast<size_t>(it - func->structures.begin());
                if (std::find(structureStack.begin(), structureStack.end(), structureIndex) != structureStack.end()) {
                    return 1;
                }

                structureStack.push_back(structureIndex);
                vm_uint32_t slots = 0;
                for (const auto fieldType : it->fieldTypes) {
                    slots += self(self, fieldType, structureStack);
                }
                structureStack.pop_back();
                return slots;
            };

            const auto reflectType = [&](vm_uint32_t packedType) {
                vm_reflect_type_t type{};
                const auto baseType = utils::exportTypeBase(packedType);
                if (baseType >= VM_VALUE_TYPE_BOOL && baseType <= VM_VALUE_TYPE_STRING) {
                    type.primitive_type = static_cast<vm_value_type_t>(baseType);
                } else if (baseType == utils::ExportTypeStructure) {
                    const auto structureName = utils::readString(utils::exportTypeDataOffset(packedType), data);
                    if (const auto it = structsByName.find(std::string(structureName)); it != structsByName.end()) {
                        type.structure = it->second;
                    }
                }
                return type;
            };

            for (auto& structure : func->structures) {
                vm_uint32_t fieldSlotIndex = 0;
                for (size_t k = 0; k < structure.fields.size(); ++k) {
                    auto& field = structure.fields[k];
                    field.type = reflectType(structure.fieldTypes[k]);
                    field.slot_index = static_cast<vm_uint16_t>(fieldSlotIndex);
                    std::vector<size_t> structureStack;
                    field.slot_count =
                        static_cast<vm_uint16_t>(countSlots(countSlots, structure.fieldTypes[k], structureStack));
                    fieldSlotIndex += field.slot_count;
                }
                structure.info.slot_count = static_cast<vm_uint16_t>(fieldSlotIndex);
            }

            vm_uint32_t paramSlotIndex = 0;
            for (size_t j = 0; j < nativeData.param_count; ++j) {
                const auto param = utils::read<utils::NativeParam>(bytecode, i, j).param;
                const auto paraName = utils::readString(param.name, data);
                func->linearParamNames.emplace_back(paraName);
                auto& reflectedParam = func->linearParams.emplace_back();
                reflectedParam.type = reflectType(param.type);
                reflectedParam.slot_index = static_cast<vm_uint16_t>(paramSlotIndex);
                std::vector<size_t> structureStack;
                reflectedParam.slot_count =
                    static_cast<vm_uint16_t>(countSlots(countSlots, param.type, structureStack));
                paramSlotIndex += reflectedParam.slot_count;
            }
            if (utils::exportTypeBase(nativeData.return_type) != VM_VALUE_TYPE_UNKNOWN) {
                func->hasReturn = true;
                func->reflectedReturn = reflectType(nativeData.return_type);
                std::vector<size_t> structureStack;
                func->reflectedReturnSlotCount = static_cast<vm_uint16_t>(
                    countSlots(countSlots, nativeData.return_type, structureStack));
            }
            auto [result, _] = _reflections.emplace(native, std::move(func));
            return result->second->fill();
        }
    }
    return nullptr;
}

void NativeManager::unregisterFunc(vm_native_h native) noexcept {
    if (auto nativePtr = _natives.get(native)) {
        nativePtr->func = nullptr;
        nativePtr->data = nullptr;
    }
}

void NativeManager::addNative(
    vm_script_h script,
    std::string_view name,
    vm_uint16_t paramCount,
    vm_uint32_t argSlotCount,
    vm_uint16_t returnSlotCount) {
    auto str = _cache.pool.insert(name);
    if (auto [handle, native] = _natives.find(str); native) {
        native->script = script;
        native->paramCount = paramCount;
        native->argSlotCount = argSlotCount;
        native->returnSlotCount = returnSlotCount;
        _reflections.erase(handle);
    } else {
        auto __ = _natives.emplace(str, str, script, nullptr, paramCount, argSlotCount, returnSlotCount);
    }
}

const vm_reflect_func_t* NativeManager::FuncReflect::fill() noexcept {
    for (auto& structure : structures) {
        for (size_t i = 0; i < structure.fields.size(); ++i) {
            structure.fields[i].name = structure.fieldNames[i].c_str();
        }
        structure.info.name = structure.name.c_str();
        structure.info.field_count = static_cast<vm_uint16_t>(structure.fields.size());
        structure.info.fields = structure.fields.data();
    }
    for (size_t i = 0; i < linearParams.size(); ++i) {
        linearParams[i].name = linearParamNames[i].c_str();
    }
    name = nameFunc.c_str();
    params = linearParams.data();
    return_type = hasReturn ? &reflectedReturn : nullptr;
    return_slot_count = reflectedReturnSlotCount;
    param_count = static_cast<vm_uint16_t>(linearParams.size());
    return this;
}

} // namespace vm
