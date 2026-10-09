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
#if defined(VM_PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(VM_PLATFORM_IOS) || defined(VM_PLATFORM_MAC_OS)
#include <mach-o/dyld.h>
#elif defined(VM_PLATFORM_LINUX)
#include <unistd.h>
#endif

constexpr vm_script_h UndefinedScript = vm_script_h{};
[[nodiscard]] UTILS_FORCE_INLINE bool operator==(vm_script_h lhs, vm_script_h rhs) noexcept {
    return lhs.handle == rhs.handle;
}
[[nodiscard]] UTILS_FORCE_INLINE bool operator!=(vm_script_h lhs, vm_script_h rhs) noexcept {
    return lhs.handle != rhs.handle;
}
[[nodiscard]] UTILS_FORCE_INLINE std::strong_ordering operator<=>(vm_script_h lhs, vm_script_h rhs) noexcept {
    return lhs.handle <=> rhs.handle;
}

namespace vm {

[[nodiscard]] UTILS_FORCE_INLINE static vm_value_t valueFromBool(bool value) noexcept {
    return vm_value_t{.type = VM_VALUE_TYPE_BOOL, .b = value};
}

[[nodiscard]] UTILS_FORCE_INLINE static vm_value_t valueFromInt32(int32_t value) noexcept {
    return vm_value_t{.type = VM_VALUE_TYPE_INT, .i32 = value};
}

[[nodiscard]] UTILS_FORCE_INLINE static vm_value_t valueFromFloat(float value) noexcept {
    return vm_value_t{.type = VM_VALUE_TYPE_FLOAT, .f32 = value};
}

[[nodiscard]] UTILS_FORCE_INLINE static vm_value_t valueFromString(vm_string_h str) noexcept {
    return vm_value_t{.type = VM_VALUE_TYPE_STRING, .str = str};
}

template <typename T>
[[nodiscard]] UTILS_FORCE_INLINE T parseString(vm_string_h str, const utils::StringPool& pool) noexcept {
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

[[nodiscard]] UTILS_FORCE_INLINE std::filesystem::path getBasePath() {
#if defined(VM_PLATFORM_WINDOWS)
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true) {
        DWORD size = GetModuleFileNameW(NULL, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (size == 0) {
            return std::filesystem::current_path();
        }
        if (size < buffer.size()) {
            buffer.resize(size);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer.data()).parent_path();
#elif defined(VM_PLATFORM_IOS) || defined(VM_PLATFORM_MAC_OS)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
        return std::filesystem::canonical(buffer.data()).parent_path();
    }
    return std::filesystem::current_path();
#elif defined(VM_PLATFORM_LINUX)
    std::vector<char> buffer(1024);
    while (true) {
        ssize_t size = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (size == -1) {
            return std::filesystem::current_path();
        }
        if (size < static_cast<ssize_t>(buffer.size())) {
            buffer[size] = '\0';
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer.data()).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

} // namespace vm

namespace std {

template <>
struct hash<vm_script_h> {
    [[nodiscard]] UTILS_FORCE_INLINE size_t operator()(vm_script_h script) const noexcept {
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
