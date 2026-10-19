#ifndef UTILS_UTILS_HPP
#define UTILS_UTILS_HPP

#if defined(_MSC_VER)
#define UTILS_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define UTILS_FORCE_INLINE __attribute__((always_inline)) inline
#else
#define UTILS_FORCE_INLINE inline
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <charconv>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef USE_FMT_LIBRARY
#include <fmt/format.h>
#else
#include <format>
namespace fmt = std;
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#endif

#include <vm/vm-script.h>
#include <vm/vm-types.h>

template <typename H>
concept IsHandle = requires(std::decay_t<H> handle) {
    handle.handle;
    requires std::integral<std::remove_cvref_t<decltype(handle.handle)>>;
};

struct UndefinedType {
    template <IsHandle H>
    [[nodiscard]] UTILS_FORCE_INLINE constexpr operator H() const noexcept {
        return H{};
    }
};

inline constexpr UndefinedType Undefined{};

template <IsHandle H>
[[nodiscard]] UTILS_FORCE_INLINE bool operator==(H lhs, H rhs) noexcept {
    return lhs.handle == rhs.handle;
}

template <IsHandle H>
[[nodiscard]] UTILS_FORCE_INLINE std::strong_ordering operator<=>(H lhs, H rhs) noexcept {
    return lhs.handle <=> rhs.handle;
}

template <IsHandle H>
[[nodiscard]] UTILS_FORCE_INLINE bool operator==(H lhs, UndefinedType /* rhs */) noexcept {
    return lhs.handle == 0;
}

template <IsHandle H>
[[nodiscard]] UTILS_FORCE_INLINE std::strong_ordering operator<=>(H lhs, UndefinedType /* rhs */) noexcept {
    return lhs.handle <=> 0;
}

namespace std {
template <IsHandle H>
struct hash<H> {
    [[nodiscard]] UTILS_FORCE_INLINE constexpr size_t operator()(H handle) const noexcept {
        return std::hash<decltype(handle.handle)>{}(handle.handle);
    }
};
} // namespace std

namespace utils {

UTILS_FORCE_INLINE void trim(std::string& s, char ch) {
    auto start = s.find_first_not_of(ch);
    if (start == std::string::npos) {
        s.clear();
        return;
    }
    auto end = s.find_last_not_of(ch);
    s = s.substr(start, end - start + 1);
}

[[nodiscard]] UTILS_FORCE_INLINE std::string getModuleName(const std::filesystem::path& filename) {
    const auto filenameStr = filename.filename().replace_extension().string();
    if (filenameStr.empty()) {
        return filenameStr;
    }
    std::string name;
    name.reserve(filenameStr.length());
    char prevCh = '\0';
    for (const char ch : filenameStr) {
        if (std::isalnum(ch)) {
            name.push_back(std::tolower(ch));
        } else if (prevCh != '_') {
            name.push_back('_');
        }
        prevCh = name.back();
    }
    trim(name, '_');
    return name;
}

[[nodiscard]] UTILS_FORCE_INLINE std::filesystem::path matchFile(const std::filesystem::path& file) {
    using namespace std::literals;
    const auto isRegularFile = [](const std::filesystem::path& path) {
        std::error_code error{};
        return std::filesystem::is_regular_file(path, error);
    };
    const auto isDirectory = [](const std::filesystem::path& path) {
        std::error_code error{};
        return std::filesystem::is_directory(path, error);
    };
    auto fullpath = file;
    fullpath.make_preferred();
    if (isRegularFile(fullpath)) {
        return fullpath;
    }
    auto actualDir = fullpath.parent_path();
    if (isDirectory(actualDir)) {
        const auto expectedModule = utils::getModuleName(fullpath);
        std::error_code error{};
        for (const auto& entry : std::filesystem::directory_iterator(actualDir, error)) {
            if (isRegularFile(entry)) {
                auto actualModule = utils::getModuleName(entry.path());
                if (actualModule == expectedModule) {
                    auto actualPath = entry.path();
                    auto actualExt = entry.path().extension().string();
                    std::string_view extView = actualExt;
                    for (const auto ext : {extView, ""sv, "adb"sv, "advb"sv, "adt"sv, "advt"sv, "adv"sv}) {
                        actualPath.replace_extension(ext);
                        if (isRegularFile(actualPath)) {
                            return actualPath;
                        }
                    }
                }
            }
        }
    }
    return {};
}

[[nodiscard]] UTILS_FORCE_INLINE std::filesystem::path
findFile(const std::filesystem::path& file, const std::span<const std::filesystem::path> dirs) {
    if (file.empty()) {
        return {};
    }
    if (file.is_absolute()) {
        return matchFile(file);
    }
    for (const auto& dir : dirs) {
        if (auto result = matchFile(dir / file); !result.empty()) {
            return result;
        }
    }
    return {};
}

template <typename AliasedType, typename Type>
[[nodiscard]] UTILS_FORCE_INLINE AliasedType aliasCast(Type ptr) noexcept {
    static_assert(std::is_pointer_v<AliasedType>, "AliasedType must be a pointer");
    static_assert(std::is_pointer_v<Type>, "Type must be a pointer");
    AliasedType result;
    memcpy(&result, &ptr, sizeof(AliasedType));
    return result;
}

[[nodiscard]] UTILS_FORCE_INLINE uint16_t swapBytes(uint16_t val) {
    if (std::endian::native == std::endian::little) {
        return val;
    }
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(val);
#else
    return (val << 8) | (val >> 8);
#endif
}

[[nodiscard]] UTILS_FORCE_INLINE uint32_t swapBytes(uint32_t val) {
    if (std::endian::native == std::endian::little) {
        return val;
    }
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(val);
#else
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0x00FF00FF);
    val = (val << 16) | (val >> 16);
    return val;
#endif
}

[[nodiscard]] UTILS_FORCE_INLINE vm_script_header_t swapBytes(const vm_script_header_t& header) noexcept {
    if (std::endian::native == std::endian::little) {
        return header;
    }
    auto result = header;
    result.version = swapBytes(result.version);
    result.checksum = swapBytes(result.checksum);
    return result;
}

[[nodiscard]] UTILS_FORCE_INLINE vm_script_table_t swapBytes(const vm_script_table_t& table) noexcept {
    if (std::endian::native == std::endian::little) {
        return table;
    }
    auto result = table;
    result.export_offset = swapBytes(result.export_offset);
    result.export_size = swapBytes(result.export_size);
    result.data_offset = swapBytes(result.data_offset);
    result.data_size = swapBytes(result.data_size);
    result.text_offset = swapBytes(result.text_offset);
    result.text_size = swapBytes(result.text_size);
    return result;
}

[[nodiscard]] UTILS_FORCE_INLINE vm_script_export_t swapBytes(const vm_script_export_t& exportTable) noexcept {
    if (std::endian::native == std::endian::little) {
        return exportTable;
    }
    auto result = exportTable;
    result.module_name = swapBytes(result.module_name);
    result.root_frame_slots = swapBytes(result.root_frame_slots);
    result.var_count = swapBytes(result.var_count);
    result.var_size = swapBytes(result.var_size);
    result.func_count = swapBytes(result.func_count);
    result.func_size = swapBytes(result.func_size);
    result.native_count = swapBytes(result.native_count);
    result.native_size = swapBytes(result.native_size);
    return result;
}

[[nodiscard]] UTILS_FORCE_INLINE vm_script_export_var_t swapBytes(const vm_script_export_var_t& exportVar) noexcept {
    if (std::endian::native == std::endian::little) {
        return exportVar;
    }
    auto result = exportVar;
    result.name = swapBytes(result.name);
    return result;
}

[[nodiscard]] UTILS_FORCE_INLINE vm_script_export_param_t swapBytes(const vm_script_export_param_t& param) noexcept {
    if (std::endian::native == std::endian::little) {
        return param;
    }
    auto result = param;
    result.name = swapBytes(result.name);
    return result;
}

[[nodiscard]] UTILS_FORCE_INLINE vm_script_export_func_t swapBytes(const vm_script_export_func_t& exportFunc) noexcept {
    if (std::endian::native == std::endian::little) {
        return exportFunc;
    }
    auto result = exportFunc;
    result.name = swapBytes(result.name);
    result.text_offset = swapBytes(result.text_offset);
    result.frame_slot_count = swapBytes(result.frame_slot_count);
    return result;
}

[[nodiscard]] UTILS_FORCE_INLINE vm_script_export_native_t
swapBytes(const vm_script_export_native_t& exportNative) noexcept {
    if (std::endian::native == std::endian::little) {
        return exportNative;
    }
    auto result = exportNative;
    result.name = swapBytes(result.name);
    return result;
}

[[nodiscard]] UTILS_FORCE_INLINE vm_uint16_t readUint16(std::span<const uint8_t> code, vm_uint32_t& offset) noexcept {
    if (offset + 2 <= code.size()) {
        const vm_uint16_t b0 = static_cast<vm_uint16_t>(code[offset + 0]) << 0;
        const vm_uint16_t b1 = static_cast<vm_uint16_t>(code[offset + 1]) << 8;
        const vm_uint16_t val = b0 | b1;
        offset += 2;
        return swapBytes(val);
    }
    return 0;
}

[[nodiscard]] UTILS_FORCE_INLINE vm_uint32_t readUint32(std::span<const uint8_t> code, vm_uint32_t& offset) noexcept {
    if (offset + 4 <= code.size()) {
        const vm_uint32_t b0 = static_cast<vm_uint32_t>(code[offset + 0]) << 0;
        const vm_uint32_t b1 = static_cast<vm_uint32_t>(code[offset + 1]) << 8;
        const vm_uint32_t b2 = static_cast<vm_uint32_t>(code[offset + 2]) << 16;
        const vm_uint32_t b3 = static_cast<vm_uint32_t>(code[offset + 3]) << 24;
        const vm_uint32_t val = b0 | b1 | b2 | b3;
        offset += 4;
        return swapBytes(val);
    }
    return 0;
}

[[nodiscard]] UTILS_FORCE_INLINE vm_sint32_t readInt32(std::span<const uint8_t> code, vm_uint32_t& offset) noexcept {
    return static_cast<vm_sint32_t>(readUint32(code, offset));
}

[[nodiscard]] UTILS_FORCE_INLINE vm_float32_t readFloat(std::span<const uint8_t> code, vm_uint32_t& offset) noexcept {
    static_assert(sizeof(vm_float32_t) == sizeof(vm_sint32_t));
    auto bits = readInt32(code, offset);
    vm_float32_t val;
    std::memcpy(&val, &bits, sizeof(vm_float32_t));
    return val;
}

[[nodiscard]] UTILS_FORCE_INLINE std::string_view
readString(vm_uint32_t offset, std::span<const uint8_t> data) noexcept {
    const auto strLen = readUint16(data, offset);
    if (offset + strLen <= data.size()) {
        return std::string_view((const char*)data.data() + offset, strLen);
    }
    return {};
}

[[nodiscard]] UTILS_FORCE_INLINE std::string_view
readString(std::span<const uint8_t> code, vm_uint32_t& offset, std::span<const uint8_t> data) noexcept {
    auto dataOffset = readUint32(code, offset);
    return readString(dataOffset, data);
}

struct SectionData {
    std::span<const uint8_t> data;
};

struct SectionText {
    std::span<const uint8_t> text;
};

struct NativeParam {
    vm_script_export_param_t param;
};

namespace internal {

template <typename T>
struct Result {
    static T get(std::span<const vm_uint8_t> bytecode, size_t offset, size_t /* size */) {
        return swapBytes(*aliasCast<const T*>(bytecode.data() + offset));
    }
};

template <>
struct Result<SectionData> {
    static SectionData get(std::span<const vm_uint8_t> bytecode, size_t offset, size_t size) {
        return SectionData{.data = {bytecode.data() + offset, size}};
    }
};

template <>
struct Result<SectionText> {
    static SectionText get(std::span<const vm_uint8_t> bytecode, size_t offset, size_t size) {
        return SectionText{.text = {bytecode.data() + offset, size}};
    }
};

template <>
struct Result<NativeParam> {
    static NativeParam get(std::span<const vm_uint8_t> bytecode, size_t offset, size_t size) {
        return {swapBytes(*aliasCast<const vm_script_export_param_t*>(bytecode.data() + offset))};
    }
};

template <typename>
struct Offset {};

template <>
struct Offset<vm_script_header_t> {
    [[nodiscard]] static constexpr std::pair<size_t, size_t>
    offset(std::span<const vm_uint8_t> /* bytecode */, size_t i, size_t j) noexcept {
        assert(i == 0u);
        assert(j == 0u);
        return {0, sizeof(vm_script_header_t)};
    }
};

template <>
struct Offset<vm_script_table_t> {
    [[nodiscard]] static constexpr std::pair<size_t, size_t>
    offset(std::span<const vm_uint8_t> /* bytecode */, size_t i, size_t j) noexcept {
        assert(i == 0u);
        assert(j == 0u);
        return {sizeof(vm_script_header_t), sizeof(vm_script_table_t)};
    }
};

template <>
struct Offset<SectionData> {
    [[nodiscard]] static std::pair<size_t, size_t>
    offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept;
};

template <>
struct Offset<SectionText> {
    [[nodiscard]] static std::pair<size_t, size_t>
    offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept;
};

template <>
struct Offset<vm_script_export_t> {
    [[nodiscard]] static std::pair<size_t, size_t>
    offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept;
};

template <>
struct Offset<vm_script_export_var_t> {
    [[nodiscard]] static std::pair<size_t, size_t>
    offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept;
};

template <>
struct Offset<vm_script_export_param_t> {
    [[nodiscard]] static std::pair<size_t, size_t>
    offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept;
};

template <>
struct Offset<vm_script_export_func_t> {
    [[nodiscard]] static std::pair<size_t, size_t>
    offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept;
};

template <>
struct Offset<vm_script_export_native_t> {
    [[nodiscard]] static std::pair<size_t, size_t>
    offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept;
};

template <>
struct Offset<NativeParam> {
    [[nodiscard]] static std::pair<size_t, size_t>
    offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept;
};

} // namespace internal

template <typename Section>
[[nodiscard]] UTILS_FORCE_INLINE Section
read(std::span<const vm_uint8_t> bytecode, size_t i = 0, size_t j = 0) noexcept {
    const auto [offset, size] = internal::Offset<Section>::offset(bytecode, i, j);
    return internal::Result<Section>::get(bytecode, offset, size);
}

UTILS_FORCE_INLINE std::pair<size_t, size_t>
internal::Offset<SectionData>::offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept {
    assert(i == 0u);
    assert(j == 0u);
    const auto result = read<vm_script_table_t>(bytecode);
    return {result.data_offset, result.data_size};
}

UTILS_FORCE_INLINE std::pair<size_t, size_t>
internal::Offset<SectionText>::offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept {
    assert(i == 0u);
    assert(j == 0u);
    const auto result = read<vm_script_table_t>(bytecode);
    return {result.text_offset, result.text_size};
}

UTILS_FORCE_INLINE std::pair<size_t, size_t>
internal::Offset<vm_script_export_t>::offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept {
    assert(i == 0u);
    assert(j == 0u);
    const auto result = read<vm_script_table_t>(bytecode);
    return {result.export_offset, result.export_size};
}

UTILS_FORCE_INLINE std::pair<size_t, size_t>
internal::Offset<vm_script_export_var_t>::offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept {
    const auto [offset, _] = internal::Offset<vm_script_export_t>::offset(bytecode, 0, 0);
    return {offset + sizeof(vm_script_export_t) + sizeof(vm_script_export_var_t) * i, sizeof(vm_script_export_var_t)};
}

UTILS_FORCE_INLINE std::pair<size_t, size_t>
internal::Offset<vm_script_export_param_t>::offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept {
    const auto [offset, _] = Offset<vm_script_export_func_t>::offset(bytecode, i, 0);
    return {
        offset + sizeof(vm_script_export_func_t) + sizeof(vm_script_export_param_t) * j,
        sizeof(vm_script_export_param_t)};
}

UTILS_FORCE_INLINE std::pair<size_t, size_t>
internal::Offset<vm_script_export_func_t>::offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept {
    const auto varCount = read<vm_script_export_t>(bytecode).var_count;
    const auto [offset, _] = internal::Offset<vm_script_export_var_t>::offset(bytecode, varCount, 0);
    size_t funcOffset = offset;
    for (size_t f = 0; f < i; ++f) {
        const auto params =
            Result<vm_script_export_func_t>::get(bytecode, funcOffset, sizeof(vm_script_export_func_t)).param_count;
        funcOffset += sizeof(vm_script_export_func_t) + params * sizeof(vm_script_export_param_t);
    }
    return {funcOffset, sizeof(vm_script_export_func_t)};
}

UTILS_FORCE_INLINE std::pair<size_t, size_t>
internal::Offset<vm_script_export_native_t>::offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept {
    const auto [offset, _] = Offset<vm_script_export_t>::offset(bytecode, 0, 0);
    const auto exports = read<vm_script_export_t>(bytecode);
    auto nativeOffset = offset + sizeof(vm_script_export_t) + exports.var_size + exports.func_size;
    for (size_t n = 0; n < i; ++n) {
        const auto params =
            Result<vm_script_export_native_t>::get(bytecode, nativeOffset, sizeof(vm_script_export_native_t))
                .param_count;
        nativeOffset += sizeof(vm_script_export_native_t) + params * sizeof(vm_script_export_param_t);
    }
    return {nativeOffset, sizeof(vm_script_export_native_t)};
}

UTILS_FORCE_INLINE std::pair<size_t, size_t>
internal::Offset<NativeParam>::offset(std::span<const vm_uint8_t> bytecode, size_t i, size_t j) noexcept {
    const auto [offset, _] = Offset<vm_script_export_native_t>::offset(bytecode, i, 0);
    return {
        offset + sizeof(vm_script_export_native_t) + sizeof(vm_script_export_param_t) * j,
        sizeof(vm_script_export_param_t)};
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

[[nodiscard]] UTILS_FORCE_INLINE uint32_t adler32(std::ranges::viewable_range auto&& view) noexcept {
    uint32_t a = 1, b = 0;
    for (const uint8_t byte : view) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

#if defined(__EMSCRIPTEN__)

namespace embind {

[[nodiscard]] UTILS_FORCE_INLINE bool isFunction(const emscripten::val& value) noexcept {
    using namespace emscripten;
    return val::global("Function")["prototype"].call<bool>("isPrototypeOf", value);
}

[[noreturn]] UTILS_FORCE_INLINE void throwError(const char* err, const char* message) {
    using namespace emscripten;
    val::global(err).new_(val::u8string(message)).throw_();
}

[[nodiscard]] UTILS_FORCE_INLINE std::vector<vm_uint8_t> readByteBuffer(const emscripten::val& input) {
    using namespace emscripten;
    if (input.isNull() || input.isUndefined()) {
        throwError("TypeError", "Input cannot be null or undefined");
    }
    val bytes = val::undefined();
    if (input.isString()) {
        val encoder = val::global("TextEncoder").new_();
        bytes = encoder.call<val>("encode", input);
    } else if (input.instanceof(val::global("ArrayBuffer"))) {
        bytes = val::global("Uint8Array").new_(input);
    } else if (!input["buffer"].isUndefined() && input["byteLength"].isNumber()) {
        bytes = val::global("Uint8Array").new_(input["buffer"], input["byteOffset"], input["byteLength"]);
    } else {
        throwError("TypeError", "Expected String, ArrayBuffer, or TypedArray");
    }
    double rawLength = bytes["length"].as<double>();
    if (!std::isfinite(rawLength) || rawLength < 0) {
        throwError("TypeError", "Invalid byte buffer length");
    }
    const auto length = static_cast<size_t>(rawLength);
    if (length == 0) {
        return std::vector<vm_uint8_t>();
    }
    return emscripten::vecFromJSArray<vm_uint8_t>(bytes);
}

} // namespace embind

#endif

} // namespace utils

#include "flat-map.hpp"
#include "object.hpp"
#include "pool.hpp"
#include "string-pool.hpp"
#include "translator.hpp"

#endif // UTILS_UTILS_HPP
