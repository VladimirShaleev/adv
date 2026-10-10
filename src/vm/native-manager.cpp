#include "native-manager.hpp"

namespace vm {

NativeManager::NativeManager(Cache& cache) noexcept : _cache(cache) {}

const Native* NativeManager::func(vm_native_h native) const noexcept { return _natives.get(native); }

vm_native_h NativeManager::find(std::string_view name) const noexcept {
    if (auto str = _cache.pool.find(name)) {
        auto [handle, _] = _natives.find(str.value());
        return handle;
    }
    return Undefined;
}

vm_native_h NativeManager::registerFunc(std::string_view name, vm_native_callback_t func, vm_data_t data) noexcept {
    try {
        auto str = _cache.pool.insert(name);
        auto [handle, _] = _natives.emplace(str, func, 0, data);
        return handle;
    } catch (const std::bad_alloc&) {
        return Undefined;
    }
}

void NativeManager::unregisterFunc(vm_native_h native) noexcept {
    if (auto nativePtr = _natives.get(native)) {
        nativePtr->func = nullptr;
        nativePtr->paramCount = 0;
        nativePtr->data = nullptr;
    }
}

} // namespace vm
