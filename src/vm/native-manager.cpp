#include "native-manager.hpp"

namespace vm {

NativeManager::NativeManager(Cache& cache) noexcept : _cache(cache) {}

void NativeManager::addNatives(std::span<const uint8_t> bytecode) {
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
        addNative(name, native.param_count, returnCount);
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
    if (auto str = _cache.pool.find(name)) {
        auto [handle, native] = _natives.find(str.value());
        if (native) {
            native->func = func;
            native->data = data;
            return handle;
        }
    }
    return Undefined;
}

void NativeManager::unregisterFunc(vm_native_h native) noexcept {
    if (auto nativePtr = _natives.get(native)) {
        nativePtr->func = nullptr;
        nativePtr->data = nullptr;
    }
}

void NativeManager::addNative(std::string_view name, vm_uint16_t paramCount, vm_uint16_t returnCount) {
    auto str = _cache.pool.insert(name);
    auto _ = _natives.emplace(str, nullptr, paramCount, returnCount);
}

} // namespace vm
