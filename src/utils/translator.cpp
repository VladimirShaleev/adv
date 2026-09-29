#include "translator.hpp"

#include <algorithm>
#include <charconv>
#include <fmt/format.h>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <unordered_map>

#include <vm/vm-opcode.h>
#include <vm/vm-script.h>
#include <vm/vm-version.h>

namespace utils {

namespace {

enum class Format {
    Unknown,
    Advt
};

enum class TokenType {
    Eof,
    Colon,
    Comment,
    Newline,
    Section,
    String,
    Identifier
};

struct Token {
    TokenType type;
    std::string_view value;
    vm_uint32_t line;
    vm_uint32_t column;
};

struct Instruction {
    vm_opcode_t opcode;
    std::string_view operand;
    std::string_view label;
    vm_uint32_t line;
};

struct Advt {
    Format format;
    vm_uint32_t version;
    vm_uint32_t checksum;
    std::unordered_map<std::string_view, std::string_view> dataSection;
    std::vector<Instruction> textSection;
};

class Lexer {
public:
    explicit Lexer(std::span<const vm_char_t> code) noexcept : _source(code.data(), code.size()) {}

    [[nodiscard]] Token nextToken() noexcept {
        skipWhitespaces();
        if (_cursor >= _source.size()) {
            return Token{TokenType::Eof, {}, _line, _column};
        }
        const auto start = _cursor;
        const auto startCol = _column;
        const auto ch = _source[_cursor];
        switch (ch) {
        case '\n':
        case '\r': {
            if (ch == '\r' && peek() == '\n') {
                advance();
            }
            advance();
            Token tok{TokenType::Newline, _source.substr(start, _cursor - start), _line, startCol};
            _line++;
            _column = 1;
            return tok;
        }
        case ';': {
            while (_cursor < _source.size() && _source[_cursor] != '\n' && _source[_cursor] != '\r') {
                advance();
            }
            return Token{TokenType::Comment, _source.substr(start, _cursor - start), _line, startCol};
        }
        case '.': {
            advance();
            while (_cursor < _source.size() && (std::isalnum(_source[_cursor]) || _source[_cursor] == '_')) {
                advance();
            }
            return Token{TokenType::Section, _source.substr(start, _cursor - start), _line, startCol};
        }
        case ':': {
            advance();
            return Token{TokenType::Colon, _source.substr(_cursor - 1, 1), _line, startCol};
        }
        case '"': {
            advance();
            while (_cursor < _source.size()) {
                if (_source[_cursor] == '\\') {
                    advance();
                    if (_cursor < _source.size()) {
                        advance();
                    }
                } else if (_source[_cursor] == '"') {
                    advance();
                    break;
                } else {
                    advance();
                }
            }
            return Token{TokenType::String, _source.substr(start, _cursor - start), _line, startCol};
        }
        }
        while (_cursor < _source.size() && !isDelimiter(_source[_cursor])) {
            advance();
        }
        return Token{TokenType::Identifier, _source.substr(start, _cursor - start), _line, startCol};
    }

private:
    void advance() noexcept {
        _cursor++;
        _column++;
    }

    [[nodiscard]] char peek() const noexcept { return _cursor + 1 < _source.size() ? _source[_cursor + 1] : '\0'; }

    void skipWhitespaces() noexcept {
        while (_cursor < _source.size()) {
            const auto c = _source[_cursor];
            if (c == ' ' || c == '\t') {
                advance();
            } else {
                break;
            }
        }
    }

    [[nodiscard]] static bool isDelimiter(char c) noexcept {
        return std::isspace(c) || c == ':' || c == ';' || c == '"';
    }

    std::string_view _source;
    uint32_t _cursor{0};
    uint32_t _line{1};
    uint32_t _column{1};
};

class Parser {
public:
    explicit Parser(Lexer& lexer) : _lexer(lexer) { advance(); }

    Advt parse() {
        using namespace std::string_view_literals;
        Advt result{};
        while (_currentToken.type != TokenType::Eof) {
            skipNewlinesAndComments();
            if (_currentToken.type == TokenType::Eof) {
                break;
            }
            if (_currentToken.type == TokenType::Section) {
                std::string_view sec = _currentToken.value;
                if (equals(sec, ".format"sv)) {
                    result.format = parseFormat();
                } else if (equals(sec, ".version"sv)) {
                    result.version = parseVersion();
                } else if (equals(sec, ".checksum"sv)) {
                    result.checksum = parseUint32();
                } else if (equals(sec, ".data")) {
                    parseDataSection(result);
                } else if (equals(sec, ".text")) {
                    parseTextSection(result);
                } else {
                    error(fmt::format("Unknown section '{}'", sec), TranslateErrorCode::UnknownSection);
                }
            } else {
                error("Expected section header starting with '.'", TranslateErrorCode::ExpectedSectionStartingWithDot);
            }
        }
        if (result.format == Format::Unknown) {
            error("Unknown format", TranslateErrorCode::UnknownFormat);
        }
        return result;
    }

private:
    void advance() noexcept {
        _prevToken = _currentToken;
        _currentToken = _lexer.nextToken();
    }

    void skipNewlinesAndComments() noexcept {
        while (_currentToken.type == TokenType::Newline || _currentToken.type == TokenType::Comment) {
            advance();
        }
    }

    void expect(TokenType expected, const std::string& message) {
        if (_currentToken.type != expected) {
            error(message, TranslateErrorCode::SyntaxError);
        }
    }

    [[noreturn]] void error(const std::string& message, TranslateErrorCode errorCode) const {
        throw TranslateError{errorCode, message, _prevToken.line, _prevToken.column};
    }

    [[nodiscard]] vm_uint32_t parseVersion() {
        auto strVer = parseHeaderValue();
        auto str = strVer;
        if (auto major = parseComponent(str, false)) {
            if (auto minor = parseComponent(str, false)) {
                if (auto micro = parseComponent(str, true)) {
                    auto version = VM_VERSION_ENCODE(major.value(), minor.value(), micro.value());
                    if ((version >> 8) == (VM_VERSION >> 8)) {
                        return version;
                    }
                }
            }
        }
        error(
            fmt::format("Script version '{}' is incompatible with adv version '{}'", strVer, VM_VERSION_STRING),
            TranslateErrorCode::IncompatibleVersion);
    }

    [[nodiscard]] Format parseFormat() {
        using namespace std::string_view_literals;
        auto str = parseHeaderValue();
        if (equals(str, "ADVT"sv)) {
            return Format::Advt;
        }
        error(fmt::format("Unknown format '{}'", str), TranslateErrorCode::UnknownFormat);
    }

    [[nodiscard]] vm_uint32_t parseUint32() {
        auto str = parseHeaderValue();
        vm_uint32_t val = 0;
        const auto [_, ec] = std::from_chars(str.data(), str.data() + str.length(), val);
        if (ec == std::errc()) {
            return val;
        } else if (ec == std::errc::result_out_of_range) {
            error(fmt::format("Value out of range '{}'", str), TranslateErrorCode::ValueOutOfRange);
        } else {
            error(fmt::format("Syntax Error '{}'", str), TranslateErrorCode::SyntaxError);
        }
    }

    [[nodiscard]] std::string_view parseHeaderValue() {
        advance();
        expect(TokenType::Colon, "Expected ':' after header name");
        advance();
        skipNewlinesAndComments();
        if (_currentToken.type != TokenType::Identifier) {
            error("Expected header value", TranslateErrorCode::ExpectedHeaderValue);
        }
        auto result = _currentToken.value;
        advance();
        return result;
    }

    void parseDataSection(Advt& result) {
        advance();
        expect(TokenType::Colon, "Expected ':' after .data");
        advance();
        while (_currentToken.type != TokenType::Eof) {
            skipNewlinesAndComments();
            if (_currentToken.type == TokenType::Section || _currentToken.type == TokenType::Eof) {
                break;
            }
            expect(TokenType::Identifier, "Expected data alias identifier");
            auto name = _currentToken.value;
            advance();
            expect(TokenType::Colon, "Expected ':' after data alias");
            advance();
            skipNewlinesAndComments();
            expect(TokenType::String, "Expected string literal in .data section");
            result.dataSection[name] = _currentToken.value;
            advance();
        }
    }

    void parseTextSection(Advt& result) {
        advance();
        expect(TokenType::Colon, "Expected ':' after .text");
        advance();
        while (_currentToken.type != TokenType::Eof) {
            skipNewlinesAndComments();
            if (_currentToken.type == TokenType::Section || _currentToken.type == TokenType::Eof) {
                break;
            }
            Instruction instruction;
            instruction.line = _currentToken.line;
            if (_currentToken.type == TokenType::Identifier) {
                auto firstToken(_currentToken.value);
                advance();
                if (_currentToken.type == TokenType::Colon) {
                    instruction.label = firstToken;
                    advance();
                    skipNewlinesAndComments();
                    if (_currentToken.type != TokenType::Identifier) {
                        result.textSection.push_back(instruction);
                        continue;
                    }
                    instruction.opcode = parseOpcode(_currentToken.value);
                    advance();
                } else {
                    instruction.opcode = parseOpcode(firstToken);
                }
            } else {
                error("Expected instruction or label", TranslateErrorCode::ExpectedInstruction);
            }
            while (_currentToken.type != TokenType::Newline && _currentToken.type != TokenType::Comment &&
                   _currentToken.type != TokenType::Eof) {
                if (_currentToken.type == TokenType::Identifier || _currentToken.type == TokenType::String) {
                    instruction.operand = _currentToken.value;
                } else {
                    error("Unexpected token in operands", TranslateErrorCode::UnexpectedTokenInOperand);
                }
                advance();
            }
            result.textSection.push_back(instruction);
        }
    }

    [[nodiscard]] static std::optional<vm_uint32_t> parseComponent(std::string_view& str, bool isLast) noexcept {
        if (str.empty()) {
            return std::nullopt;
        }
        if (str.size() > 1 && str[0] == '0' && str[1] >= '0' && str[1] <= '9') {
            return std::nullopt;
        }
        vm_sint32_t value = 0;
        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
        if (ec != std::errc{} || value < 0) {
            return std::nullopt;
        }
        if (isLast) {
            if (ptr != str.data() + str.size()) {
                return std::nullopt;
            }
        } else {
            if (ptr == str.data() + str.size() || *ptr != '.') {
                return std::nullopt;
            }
            str.remove_prefix((ptr - str.data()) + 1);
        }
        return value;
    }

    [[nodiscard]] vm_opcode_t parseOpcode(std::string_view str) {
        char buffer[64] = "VM_OPCODE_";
        if (str.length() + 10 < sizeof(buffer)) {
            std::memcpy(buffer + 10, str.data(), str.length());
            auto opcode = magic_enum::enum_cast<vm_opcode_t>({buffer, str.length() + 10}, magic_enum::case_insensitive);
            if (opcode) {
                return opcode.value();
            }
        }
        error(fmt::format("Unknown opcode '{}'", str), TranslateErrorCode::UnknownOpcode);
    }

    [[nodiscard]] static bool equals(std::string_view lhs, std::string_view rhs) noexcept {
        return std::ranges::equal(lhs, rhs, [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        });
    }

    Lexer& _lexer;
    Token _prevToken;
    Token _currentToken;
};

} // namespace

[[nodiscard]] bool isBinaryCode(std::span<const vm_char_t> code) noexcept {
    if (code.size() >= sizeof(vm_script_header_t)) {
        auto ptr = code.data();
        vm_script_header_t* header;
        memcpy((void*)&header, (void*)&ptr, sizeof(vm_script_header_t*));

        const auto magic = header->magic;
        if (magic[0] == 'a' && magic[1] == 'd' && magic[2] == 'v' && magic[3] == 'b') {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::vector<uint8_t> translateTextToBinary(std::span<const char> text, TranslateError& error) noexcept {
    Lexer lexer(text);
    Parser parser(lexer);
    try {
        error = {};
        auto advt = parser.parse();
        int i = 4;
    } catch (const TranslateError& err) {
        error = err;
    }
    return {};
}

} // namespace utils
