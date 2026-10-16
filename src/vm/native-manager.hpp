#ifndef VM_NATIVE_MANAGER_HPP
#define VM_NATIVE_MANAGER_HPP

#include "cache.hpp"

namespace vm {

struct Native {
    vm_string_h name;
    vm_script_h script;
    vm_native_callback_t func;
    vm_uint16_t paramCount;
    vm_uint16_t returnCount;
    vm_data_t data;
};

class NativeManager {
public:
    NativeManager(Cache& cache) noexcept;

    void addNatives(vm_script_h script, std::span<const uint8_t> bytecode);

    [[nodiscard]] const Native* func(vm_native_h native) const noexcept;

    [[nodiscard]] vm_native_h find(std::string_view name) const noexcept;

    [[nodiscard]] vm_native_h registerFunc(std::string_view name, vm_native_callback_t func, vm_data_t data) noexcept;

    [[nodiscard]] vm_script_h script(vm_native_h native) const noexcept;

    [[nodiscard]] const vm_reflect_func_t* reflect(vm_native_h native, std::span<const uint8_t> bytecode);

    void unregisterFunc(vm_native_h native) noexcept;

private:
    void addNative(vm_script_h script, std::string_view name, vm_uint16_t paramCount, vm_uint16_t returnCount);

    struct FuncReflect : vm_reflect_func_t {
        std::string nameFunc;
        std::vector<std::string> linearParamNames;
        std::vector<vm_reflect_param_t> linearParams;
        std::vector<vm_value_type_t> linearReturns;

        const vm_reflect_func_t* fill() noexcept;
    };

    Cache& _cache;
    utils::FlatMap<vm_string_h, vm_native_h, Native> _natives;
    std::unordered_map<vm_native_h, std::unique_ptr<FuncReflect>> _reflections;
};

} // namespace vm

#endif // VM_NATIVE_MANAGER_HPP
