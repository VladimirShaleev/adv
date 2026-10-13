#ifndef UTILS_TRANSLATOR_HPP
#define UTILS_TRANSLATOR_HPP

#include "utils.hpp"

namespace utils {

enum Format {
    Unknown,
    Advb,
    Advt
};

struct TranslateResult {
    enum ErrorCode {
        NoError = 0,
        OutOfMemory,
        SyntaxError,
        UnknownError,
        UnknownFormat,
        UnknownOpcode,
        UnknownIdentifier,
        InvalidIdentifierReference,
        RedefinedIdentifier,
        IncompatibleVersion,
        ValueOutOfRange,
        UnknownSection,
        ExpectedSectionStartingWithDot,
        ExpectedHeaderValue,
        ExpectedInstruction,
        UnexpectedTokenInOperand
    };

    Format format;
    ErrorCode errorCode;
    std::string message;
    uint32_t line;
    uint32_t column;
};

[[nodiscard]] bool isBinaryCode(std::span<const char> code, bool checkSize = true) noexcept;

[[nodiscard]] bool
validateChecksum(std::span<const char> code, uint32_t* actual = nullptr, uint32_t* expected = nullptr) noexcept;

[[nodiscard]] bool
validateVersion(std::span<const char> code, uint32_t expectedVersion, uint32_t* compiledVersion = nullptr) noexcept;

[[nodiscard]] std::vector<uint8_t> translateTextToBinary(std::span<const char> text, TranslateResult& error) noexcept;

} // namespace utils

#endif // UTILS_TRANSLATOR_HPP
