#include "translator.hpp"
#include "utils.hpp"

#include <algorithm>
#include <charconv>
#include <fmt/format.h>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <type_traits>
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

struct PatchInstruction {
    std::string_view identifier;
    vm_uint32_t offset;
    vm_uint32_t line;
};

struct Advt {
    static constexpr vm_uint32_t offsetData =
        static_cast<vm_uint32_t>(sizeof(vm_script_header_t) + sizeof(vm_script_table_t));

    Format format;
    vm_uint32_t version;
    vm_uint32_t checksum;
    std::unordered_map<std::string_view, std::string_view> identifierMap;
    std::vector<PatchInstruction> identifiers;
    std::vector<PatchInstruction> labels;
    std::unordered_map<std::string_view, vm_uint32_t> labelMap;
    std::unordered_map<std::string_view, vm_uint32_t> dataSectionMap;
    std::vector<uint8_t> headerAndDataSection;
    std::vector<uint8_t> textSection;
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

    std::vector<uint8_t> parse() {
        using namespace std::string_view_literals;
        Advt result{};
        result.headerAndDataSection.resize(Advt::offsetData);
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
                    result.checksum = parseValue<vm_uint32_t>(parseHeaderValue());
                } else if (equals(sec, ".data")) {
                    parseDataSection(result);
                } else if (equals(sec, ".text")) {
                    parseTextSection(result);
                } else {
                    error(fmt::format("Unknown section '{}'", sec), TranslateError::UnknownSection);
                }
            } else {
                error("Expected section header starting with '.'", TranslateError::ExpectedSectionStartingWithDot);
            }
        }
        if (result.format == Format::Unknown) {
            error("Unknown format", TranslateError::UnknownFormat);
        }
        resolveIdentifiers(result);
        return mergeTextSection(result);
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
            error(message, TranslateError::SyntaxError);
        }
    }

    [[noreturn]] void error(
        const std::string& message,
        TranslateError::ErrorCode errorCode,
        std::optional<vm_uint32_t> line = std::nullopt) const {
        throw TranslateError{errorCode, message, line.value_or(_prevToken.line), line ? 0 : _prevToken.column};
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
            TranslateError::IncompatibleVersion);
    }

    [[nodiscard]] Format parseFormat() {
        using namespace std::string_view_literals;
        auto str = parseHeaderValue();
        if (equals(str, "ADVT"sv)) {
            return Format::Advt;
        }
        error(fmt::format("Unknown format '{}'", str), TranslateError::UnknownFormat);
    }

    template <typename T>
    [[nodiscard]] T parseValue(std::string_view str) const {
        static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);
        T val{};
        const auto [_, ec] = std::from_chars(str.data(), str.data() + str.length(), val);
        if (ec == std::errc()) {
            return val;
        } else if (ec == std::errc::result_out_of_range) {
            error(fmt::format("Value out of range '{}'", str), TranslateError::ValueOutOfRange);
        } else {
            error(fmt::format("Syntax Error '{}'", str), TranslateError::SyntaxError);
        }
    }

    [[nodiscard]] std::string_view parseHeaderValue() {
        advance();
        expect(TokenType::Colon, "Expected ':' after header name");
        advance();
        skipNewlinesAndComments();
        if (_currentToken.type != TokenType::Identifier) {
            error("Expected header value", TranslateError::ExpectedHeaderValue);
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
            if (result.identifierMap.contains(_currentToken.value)) {
                error(
                    fmt::format("Duplicate identifier '{}'", _currentToken.value), TranslateError::RedefinedIdentifier);
            }
            result.identifierMap[name] = _currentToken.value;
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
                        pushInstruction(result, instruction);
                        continue;
                    }
                    instruction.opcode = parseOpcode(_currentToken.value);
                    advance();
                } else {
                    instruction.opcode = parseOpcode(firstToken);
                }
            } else {
                error("Expected instruction or label", TranslateError::ExpectedInstruction);
            }
            while (_currentToken.type != TokenType::Newline && _currentToken.type != TokenType::Comment &&
                   _currentToken.type != TokenType::Eof) {
                if (_currentToken.type == TokenType::Identifier || _currentToken.type == TokenType::String) {
                    instruction.operand = _currentToken.value;
                } else {
                    error("Unexpected token in operands", TranslateError::UnexpectedTokenInOperand);
                }
                advance();
            }
            pushInstruction(result, instruction);
        }
    }

    void pushInstruction(Advt& result, const Instruction& instruction) {
        const auto label = instruction.label;
        const auto opcode = instruction.opcode;
        const auto operand = instruction.operand;
        if (!label.empty()) {
            if (result.labelMap.contains(label)) {
                error(fmt::format("Duplicate label identifier '{}'", label), TranslateError::RedefinedIdentifier);
            }
            result.labelMap[label] = pollOffset(result.textSection);
        }
        result.textSection.push_back((uint8_t)opcode);
        switch (instruction.opcode) {
        case VM_OPCODE_PUSH_LOCAL:
        case VM_OPCODE_POP_LOCAL:
            pushUint16(parseValue<vm_uint16_t>(operand), result.textSection);
            break;
        case VM_OPCODE_JUMP:
        case VM_OPCODE_JUMP_IF_FALSE:
            result.labels.emplace_back(operand, pollOffset(result.textSection), instruction.line);
            pushUint32(0, result.textSection);
            break;
        case VM_OPCODE_PUSH_INT:
            pushInt32(parseValue<vm_sint32_t>(operand), result.textSection);
            break;
        case VM_OPCODE_PUSH_FLOAT:
            pushFloat(parseValue<vm_float32_t>(operand), result.textSection);
            break;
        case VM_OPCODE_PUSH_STRING:
        case VM_OPCODE_PUSH_GLOBAL_BY_NAME:
        case VM_OPCODE_POP_GLOBAL_BY_NAME:
            if (!operand.empty() && operand[0] == '"') {
                if (auto it = result.dataSectionMap.find(operand); it != result.dataSectionMap.end()) {
                    pushUint32(it->second, result.textSection);
                } else {
                    const auto offset = pushStringData(operand, result.headerAndDataSection, Advt::offsetData);
                    result.dataSectionMap[operand] = offset;
                    pushUint32(offset, result.textSection);
                }
            } else {
                result.identifiers.emplace_back(operand, pollOffset(result.textSection), instruction.line);
                pushUint32(0, result.textSection);
            }
            break;
        default:
            break;
        }
    }

    void resolveIdentifiers(Advt& result) {
        for (const auto& [id, offsetText, line] : result.identifiers) {
            auto it = result.identifierMap.find(id);
            if (it == result.identifierMap.end()) {
                error(fmt::format("Identifier '{}' not found", id), TranslateError::UnknownIdentifier, line);
            }
            auto str = it->second;
            if (auto m = result.dataSectionMap.find(str); m != result.dataSectionMap.end()) {
                patchUint32(offsetText, m->second, result.textSection);
            } else {
                const auto offset = pushStringData(str, result.headerAndDataSection, Advt::offsetData);
                result.dataSectionMap[str] = offset;
                patchUint32(offsetText, offset, result.textSection);
            }
        }
        for (const auto& [id, offsetText, line] : result.labels) {
            auto it = result.labelMap.find(id);
            if (it == result.labelMap.end()) {
                error(fmt::format("label identifier '{}' not found", id), TranslateError::UnknownIdentifier, line);
            }
            const auto offset = it->second;
            patchUint32(offsetText, offset, result.textSection);
        }
    }

    std::vector<vm_uint8_t> mergeTextSection(Advt& result) {
        std::vector<vm_uint8_t> bytecode = std::move(result.headerAndDataSection);
        const auto offsetText = bytecode.size();
        bytecode.resize(offsetText + result.textSection.size());
        auto header = reinterpret_cast<vm_script_header_t*>(bytecode.data());
        auto table = reinterpret_cast<vm_script_table_t*>(bytecode.data() + sizeof(vm_script_header_t));
        auto text = bytecode.data() + offsetText;

        header->magic[0] = 'a';
        header->magic[1] = 'd';
        header->magic[2] = 'v';
        header->magic[3] = 'b';
        header->checksum = 0;
        header->version = VM_VERSION;
        table->data_offset = Advt::offsetData;
        table->data_size = static_cast<vm_uint32_t>(offsetText) - Advt::offsetData;
        table->text_offset = offsetText;
        table->text_size = static_cast<vm_uint32_t>(result.textSection.size());
        if (std::endian::native != std::endian::little) {
            header->version = swap_bytes(header->version);
            table->data_offset = swap_bytes(table->data_offset);
            table->data_size = swap_bytes(table->data_size);
            table->text_offset = swap_bytes(table->text_offset);
            table->text_size = swap_bytes(table->text_size);
        }
        std::memcpy(text, result.textSection.data(), result.textSection.size());
        header->checksum = adler32({bytecode.data(), bytecode.size()});
        if (std::endian::native != std::endian::little) {
            header->checksum = swap_bytes(header->checksum);
        }
        return bytecode;
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
        error(fmt::format("Unknown opcode '{}'", str), TranslateError::UnknownOpcode);
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

    [[nodiscard]] static bool equals(std::string_view lhs, std::string_view rhs) noexcept {
        return std::ranges::equal(lhs, rhs, [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        });
    }

    static void pushUint16(vm_uint16_t val, std::vector<uint8_t>& buffer) {
        val = std::endian::native == std::endian::little ? val : swap_bytes(val);
        buffer.push_back(static_cast<uint8_t>((val >> 0) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    }

    static void pushUint32(vm_uint32_t val, std::vector<uint8_t>& buffer) {
        val = std::endian::native == std::endian::little ? val : swap_bytes(val);
        buffer.push_back(static_cast<uint8_t>((val >> 0) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    }

    static void pushInt32(vm_sint32_t val, std::vector<uint8_t>& buffer) {
        pushUint32(static_cast<vm_uint32_t>(val), buffer);
    }

    static void pushFloat(vm_float32_t val, std::vector<uint8_t>& buffer) {
        static_assert(sizeof(vm_float32_t) == sizeof(vm_uint32_t));
        vm_uint32_t bits;
        std::memcpy(&bits, &val, sizeof(vm_float32_t));
        pushUint32(bits, buffer);
    }

    static void patchUint32(vm_uint32_t offset, vm_uint32_t val, std::vector<uint8_t>& buffer) {
        val = std::endian::native == std::endian::little ? val : swap_bytes(val);
        buffer[offset + 0] = static_cast<uint8_t>((val >> 0) & 0xFF);
        buffer[offset + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
        buffer[offset + 2] = static_cast<uint8_t>((val >> 16) & 0xFF);
        buffer[offset + 3] = static_cast<uint8_t>((val >> 24) & 0xFF);
    }

    [[nodiscard]] static vm_uint32_t
    pushStringData(std::string_view str, std::vector<uint8_t>& buffer, vm_uint32_t offsetData) {
        const auto offset = static_cast<vm_uint32_t>(buffer.size()) - offsetData;
        pushUint16(static_cast<vm_uint16_t>(str.length() - 2), buffer);
        buffer.insert(buffer.end(), str.begin() + 1, str.end() - 1);
        buffer.push_back(uint8_t('\0'));
        return offset;
    }

    [[nodiscard]] static vm_uint32_t pollOffset(std::vector<uint8_t>& buffer) noexcept {
        return static_cast<vm_uint32_t>(buffer.size());
    }

    Lexer& _lexer;
    Token _prevToken;
    Token _currentToken;
};

} // namespace

bool isBinaryCode(std::span<const char> code) noexcept {
    if (code.size() < sizeof(vm_script_header_t) + sizeof(vm_script_table_t)) {
        return false;
    }
    auto ptrHeader = code.data();
    vm_script_header_t* header;
    memcpy((void*)&header, (void*)&ptrHeader, sizeof(vm_script_header_t*));

    const auto magic = header->magic;
    if (magic[0] != 'a' || magic[1] != 'd' || magic[2] != 'v' || magic[3] != 'b') {
        return false;
    }

    vm_script_table_t table;
    std::memcpy(&table, code.data() + sizeof(vm_script_header_t), sizeof(table));
    if (std::endian::native != std::endian::little) {
        table.data_offset = swap_bytes(table.data_offset);
        table.data_size = swap_bytes(table.data_size);
        table.text_offset = swap_bytes(table.text_offset);
        table.text_size = swap_bytes(table.text_size);
    }

    const auto dataLeft = table.data_offset;
    const auto dataRight = dataLeft + table.data_size;
    const auto textLeft = table.text_offset;
    const auto textRight = textLeft + table.text_size;

    return dataRight <= code.size() && textRight <= code.size() && (dataRight <= textLeft || textRight <= dataLeft);
}

std::vector<uint8_t> translateTextToBinary(std::span<const char> text, TranslateError& error) noexcept {
    try {
        error = {};
        Lexer lexer(text);
        Parser parser(lexer);
        return parser.parse();
    } catch (const TranslateError& err) {
        error = err;
    } catch (const std::bad_alloc&) {
        error = {TranslateError::OutOfMemory, "Out of memory"};
    } catch (...) {
        error = {TranslateError::UnknownError, "Unknown error"};
    }
    return {};
}

} // namespace utils
