#ifndef UTILS_UTILS_HPP
#define UTILS_UTILS_HPP

#if defined(_MSC_VER)
#define UTILS_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define UTILS_FORCE_INLINE __attribute__((always_inline)) inline
#else
#define UTILS_FORCE_INLINE inline
#endif

#include "object.hpp"
#include "pool.hpp"
#include "string-pool.hpp"
#include "translator.hpp"

#include <bit>
#include <cstdint>
#include <string>
#include <string_view>

namespace utils {

UTILS_FORCE_INLINE uint16_t swap_bytes(uint16_t val) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(val);
#else
    return (val << 8) | (val >> 8);
#endif
}

UTILS_FORCE_INLINE uint32_t swap_bytes(uint32_t val) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(val);
#else
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0x00FF00FF);
    val = (val << 16) | (val >> 16);
    return val;
#endif
}

[[nodiscard]] inline std::string escape(std::string_view input) {
    std::string result;
    result.reserve(input.size() * 2);
    for (char ch : input) {
        switch (ch) {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result += ch;
            break;
        }
    }
    return result;
}

[[nodiscard]] inline std::optional<std::string> unescape(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        char ch = input[i];
        if (ch == '\\') {
            if (i + 1 >= input.size()) {
                return std::nullopt;
            }
            char nextCh = input[++i];
            switch (nextCh) {
            case '\\':
                result += '\\';
                break;
            case '"':
                result += '"';
                break;
            case 'r':
                result += '\r';
                break;
            case 'n':
                result += '\n';
                break;
            case 't':
                result += '\t';
                break;
            default:
                return std::nullopt;
            }
        } else {
            result += ch;
        }
    }
    return result;
}

} // namespace utils

#endif // UTILS_UTILS_HPP
