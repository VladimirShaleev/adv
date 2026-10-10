#ifndef VM_NATIVE_MANAGER_HPP
#define VM_NATIVE_MANAGER_HPP

#include "cache.hpp"

namespace vm {

struct Native {
    vm_native_callback_t func;
    vm_uint16_t paramCount;
    vm_data_t data;
};

class NativeManager {
public:
    explicit NativeManager(Cache& cache) noexcept;

    // [[nodiscard]] vm_native_h registerFunc();

private:
    Cache& _cache;
    utils::FlatMap<vm_string_h, vm_native_h, Native> _natives;
};

} // namespace vm

#endif // VM_NATIVE_MANAGER_HPP
