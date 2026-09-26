#ifndef ADV_COMPILER_HPP
#define ADV_COMPILER_HPP

#ifndef ADV_STATIC_BUILD
#ifdef _MSC_VER
#define ADV_API __declspec(dllexport)
#elif __GNUC__ >= 4
#define ADV_API __attribute__((visibility("default")))
#else
#define ADV_API
#endif
#endif

#include <adv/adv.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <magic_enum/magic_enum.hpp>
#include <utils/utils.hpp>

UTILS_FORCE_INLINE adv_uint16_t swap_bytes(adv_uint16_t val) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(val);
#else
    return (val << 8) | (val >> 8);
#endif
}

UTILS_FORCE_INLINE adv_uint32_t swap_bytes(adv_uint32_t val) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(val);
#else
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0x00FF00FF);
    val = (val << 16) | (val >> 16);
    return val;
#endif
}

namespace utils {
template <>
struct Result<adv_result_t> {
    static constexpr adv_result_t Success = ADV_RESULT_SUCCESS;
    static constexpr adv_result_t Error = ADV_RESULT_ERROR_UNKNOWN;
    static constexpr adv_result_t OutOfMemory = ADV_RESULT_ERROR_OUT_OF_MEMORY;
};
} // namespace utils

#endif // ADV_COMPILER_HPP
