#include "native-manager.hpp"

namespace vm {

NativeManager::NativeManager(Cache& cache) noexcept : _cache(cache) {}

void NativeManager::addNatives(vm_script_h script, std::span<const uint8_t> bytecode) {
    const auto data = utils::read<utils::SectionData>(bytecode).data;
    const auto exports = utils::read<vm_script_export_t>(bytecode);
    const size_t nativeCount = exports.native_count;
    for (size_t i = 0; i < nativeCount; ++i) {
        const auto native = utils::read<vm_script_export_native_t>(bytecode, i);
        auto name = utils::readString(native.name, data);
        auto returnCount = 0;
        if (native.return_type >= VM_VALUE_TYPE_BOOL && native.return_type <= VM_VALUE_TYPE_STRING) {
            returnCount = 1;
        }
        addNative(script, name, native.param_count, returnCount);
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
        auto [resultHandle, _] = _natives.emplace(str, str, Undefined, func, 0, 0, data);
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
            for (size_t j = 0; j < nativeData.param_count; ++j) {
                const auto param = utils::read<utils::NativeParam>(bytecode, i, j).param;
                const auto paraName = utils::readString(param.name, data);
                func->linearParamNames.emplace_back(paraName);
                func->linearParams.emplace_back(nullptr, static_cast<vm_value_type_t>(param.type));
            }
            if (nativeData.return_type >= VM_VALUE_TYPE_BOOL && nativeData.return_type <= VM_VALUE_TYPE_STRING) {
                func->linearReturns.emplace_back(static_cast<vm_value_type_t>(nativeData.return_type));
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
    vm_script_h script, std::string_view name, vm_uint16_t paramCount, vm_uint16_t returnCount) {
    auto str = _cache.pool.insert(name);
    if (auto [handle, native] = _natives.find(str); native) {
        native->script = script;
        native->paramCount = paramCount;
        native->returnCount = returnCount;
        _reflections.erase(handle);
    } else {
        auto __ = _natives.emplace(str, str, script, nullptr, paramCount, returnCount);
    }
}

const vm_reflect_func_t* NativeManager::FuncReflect::fill() noexcept {
    for (size_t i = 0; i < linearParams.size(); ++i) {
        linearParams[i].name = linearParamNames[i].c_str();
    }
    name = nameFunc.c_str();
    params = linearParams.data();
    returns = linearReturns.data();
    param_count = static_cast<vm_uint16_t>(linearParams.size());
    return_count = static_cast<vm_uint16_t>(linearReturns.size());
    return this;
}

} // namespace vm
