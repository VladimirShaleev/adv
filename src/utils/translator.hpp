#ifndef UTILS_TRANSLATOR_HPP
#define UTILS_TRANSLATOR_HPP

#include <span>
#include <string>
#include <vector>

namespace utils {

enum class TranslateErrorCode {
    NoError = 0,
    SyntaxError,
    UnknownFormat,
    UnknownOpcode,
    IncompatibleVersion,
    ValueOutOfRange,
    UnknownSection,
    ExpectedSectionStartingWithDot,
    ExpectedHeaderValue,
    ExpectedInstruction,
    UnexpectedTokenInOperand
};

struct TranslateError {
    TranslateErrorCode errorCode;
    std::string message;
    uint32_t line;
    uint32_t column;
};

[[nodiscard]] bool isBinaryCode(std::span<const uint8_t> code) noexcept;

[[nodiscard]] std::vector<uint8_t> translateTextToBinary(std::span<const char> text, TranslateError& error) noexcept;

} // namespace utils

#endif // UTILS_TRANSLATOR_HPP
