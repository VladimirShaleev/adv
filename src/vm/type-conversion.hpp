#ifndef VM_TYPE_CONVERSION_HPP
#define VM_TYPE_CONVERSION_HPP

#include "cache.hpp"

namespace vm {

template <typename From, typename To>
struct Cast {};

template <>
struct Cast<bool, vm_sint32_t> {
    [[nodiscard]] static vm_value_t cast(const vm_value_t& value, Cache& /* cache */) noexcept {
        return valueFromInt32(value.b ? 1 : 0);
    }
};

template <>
struct Cast<bool, vm_float32_t> {
    [[nodiscard]] static vm_value_t cast(const vm_value_t& value, Cache& /* cache */) noexcept {
        return valueFromFloat(value.b ? 1.0f : 0.0f);
    }
};

template <>
struct Cast<bool, vm_string_h> {
    [[nodiscard]] static vm_value_t cast(const vm_value_t& value, Cache& cache) noexcept {
        return valueFromString(value.b ? cache.strTrue : cache.strFalse);
    }
};

template <>
struct Cast<vm_sint32_t, bool> {
    [[nodiscard]] static vm_value_t cast(const vm_value_t& value, Cache& /* cache */) noexcept {
        return valueFromBool(value.i32 != 0);
    }
};

template <>
struct Cast<vm_sint32_t, vm_float32_t> {
    [[nodiscard]] static vm_value_t cast(const vm_value_t& value, Cache& /* cache */) noexcept {
        return valueFromFloat(value.i32);
    }
};

template <>
struct Cast<vm_sint32_t, vm_string_h> {
    [[nodiscard]] static vm_value_t cast(const vm_value_t& value, Cache& cache) noexcept {
        if (value.i32 < cache.strInts.size()) {
            return valueFromString(cache.strInts[value.i32]);
        } else {
            return valueFromString(cache.pool.insert(std::to_string(value.i32)));
        }
    }
};

template <>
struct Cast<vm_float32_t, bool> {
    [[nodiscard]] static vm_value_t cast(const vm_value_t& value, Cache& /* cache */) noexcept {
        return valueFromBool(value.f32 != 0.0f);
    }
};

template <>
struct Cast<vm_float32_t, vm_sint32_t> {
    [[nodiscard]] static vm_value_t cast(const vm_value_t& value, Cache& /* cache */) noexcept {
        return valueFromInt32(static_cast<vm_sint32_t>(value.f32));
    }
};

template <>
struct Cast<vm_float32_t, vm_string_h> {
    [[nodiscard]] static vm_value_t cast(const vm_value_t& value, Cache& cache) noexcept {
        auto str = fmt::format("{:g}", value.f32);
        if (str.find_last_of('.') == std::string::npos) {
            using namespace std::literals;
            str += ".0"s;
        }
        return valueFromString(cache.pool.insert(str));
    }
};

template <>
struct Cast<vm_string_h, bool> {
    [[nodiscard]] static vm_value_t cast(const vm_value_t& value, Cache& /* cache */) noexcept {
        return valueFromBool(value.str.handle != 0);
    }
};

template <>
struct Cast<vm_string_h, vm_sint32_t> {
    [[nodiscard]] static vm_value_t cast(const vm_value_t& value, Cache& cache) noexcept {
        return valueFromInt32(parseString<vm_sint32_t>(value.str, cache.pool));
    }
};

template <>
struct Cast<vm_string_h, vm_float32_t> {
    [[nodiscard]] static vm_value_t cast(const vm_value_t& value, Cache& cache) noexcept {
        return valueFromFloat(parseString<vm_float32_t>(value.str, cache.pool));
    }
};

} // namespace vm

#endif // VM_TYPE_CONVERSION_HPP
