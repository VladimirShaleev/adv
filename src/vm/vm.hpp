#ifndef VM_VM_HPP
#define VM_VM_HPP

#ifndef VM_STATIC_BUILD
#ifdef _MSC_VER
#define VM_API __declspec(dllexport)
#elif __GNUC__ >= 4
#define VM_API __attribute__((visibility("default")))
#else
#define VM_API
#endif
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <compare>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <regex>
#include <span>
#include <unordered_map>
#include <vector>

#include "vm/vm.h"

#include <utils/utils.hpp>

[[nodiscard]] inline static vm_value_t valueFromBool(bool value) noexcept {
    return vm_value_t{.type = VM_VALUE_TYPE_BOOL, .b = value};
}

[[nodiscard]] inline static vm_value_t valueFromInt32(int32_t value) noexcept {
    return vm_value_t{.type = VM_VALUE_TYPE_INT, .i32 = value};
}

[[nodiscard]] inline static vm_value_t valueFromFloat(float value) noexcept {
    return vm_value_t{.type = VM_VALUE_TYPE_FLOAT, .f32 = value};
}

[[nodiscard]] inline static vm_value_t valueFromString(vm_string_h str) noexcept {
    return vm_value_t{.type = VM_VALUE_TYPE_STRING, .str = str};
}

template <typename T>
[[nodiscard]] inline T parseString(vm_string_h str, const utils::StringPool& pool) noexcept {
    T val{};
    auto view = pool[str];
    const auto [_, ec] = std::from_chars(view.data(), view.data() + view.length(), val);
    if (ec == std::errc()) {
        return val;
    } else if (ec == std::errc::result_out_of_range) {
        // TODO: log
    } else {
        // TODO: log
    }
    return T{};
}

constexpr vm_script_h UndefinedScript = vm_script_h{};
[[nodiscard]] inline bool operator==(vm_script_h lhs, vm_script_h rhs) noexcept { return lhs.handle == rhs.handle; }
[[nodiscard]] inline bool operator!=(vm_script_h lhs, vm_script_h rhs) noexcept { return lhs.handle != rhs.handle; }
[[nodiscard]] inline std::strong_ordering operator<=>(vm_script_h lhs, vm_script_h rhs) noexcept {
    return lhs.handle <=> rhs.handle;
}

namespace std {

template <>
struct hash<vm_script_h> {
    [[nodiscard]] size_t operator()(vm_script_h script) const noexcept {
        hash<uint32_t> handleHash;
        return handleHash(script.handle);
    }
};

} // namespace std

namespace utils {
template <>
struct Result<vm_result_t> {
    static constexpr vm_result_t Success = VM_RESULT_SUCCESS;
    static constexpr vm_result_t Error = VM_RESULT_ERROR_UNKNOWN;
    static constexpr vm_result_t OutOfMemory = VM_RESULT_ERROR_OUT_OF_MEMORY;
};
} // namespace utils

#endif // VM_VM_HPP
