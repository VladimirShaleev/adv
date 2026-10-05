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
#include <fstream>
#include <regex>
#include <span>
#include <unordered_map>
#include <vector>

#include "vm/vm.h"

#include <utils/utils.hpp>

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
