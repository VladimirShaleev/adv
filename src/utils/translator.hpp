#ifndef UTILS_TRANSLATOR_HPP
#define UTILS_TRANSLATOR_HPP

#include <span>
#include <string>
#include <vector>

namespace utils {

enum Format {
    Unknown,
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

[[nodiscard]] std::vector<uint8_t> translateTextToBinary(std::span<const char> text, TranslateResult& error) noexcept;

} // namespace utils

#endif // UTILS_TRANSLATOR_HPP
