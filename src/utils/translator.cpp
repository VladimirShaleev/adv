#include "translator.hpp"

#include <vm/vm-opcode.h>
#include <vm/vm-script.h>
#include <vm/vm-value.h>
#include <vm/vm-version.h>

#include <algorithm>
#include <unordered_set>

using namespace std::string_view_literals;

namespace utils {

namespace {

enum class TokenType {
    Eof,
    Colon,
    Comma,
    Comment,
    LBrace,
    LParen,
    Newline,
    RBrace,
    RParen,
    Semicolon,
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
    std::string_view operands[4];
    std::string_view label;
    vm_uint32_t line;
};

struct PatchInstruction {
    std::string_view identifier;
    vm_uint32_t offset;
    vm_uint32_t line;
};

enum class SymbolKind {
    Variable,
    Native,
    Function,
    Structure,
    Data
};

struct SymbolType {
    vm_uint32_t code;
    std::string_view structureName;
};

struct SymbolParameter {
    std::string_view name;
    SymbolType type;
};

struct Symbol {
    SymbolKind kind;
    std::string_view name;
    vm_uint8_t type{VM_VALUE_TYPE_UNKNOWN};
    std::string_view structureName;
    std::vector<SymbolParameter> parameters;
    std::vector<SymbolParameter> fields;
    vm_uint32_t offset{};
    vm_uint32_t line{0};
};

struct Advt {
    static constexpr vm_uint32_t offsetExport =
        static_cast<vm_uint32_t>(sizeof(vm_script_header_t) + sizeof(vm_script_table_t));

    Format format;
    vm_uint32_t version;
    std::map<std::string_view, Symbol> symbolMap;
    std::unordered_map<std::string_view, vm_uint32_t> dataMap;
    std::unordered_map<std::string_view, vm_uint32_t> labelMap;
    std::vector<PatchInstruction> pushData;
    std::vector<PatchInstruction> jumps;
    std::vector<PatchInstruction> calls;
    std::vector<uint8_t> dataSection;
    std::vector<uint8_t> exportSection;
    std::vector<uint8_t> textSection;
    std::optional<uint32_t> moduleName;
    vm_uint16_t rootFrameSlots{0};
    vm_uint32_t moduleLine{0};
};

class Lexer {
public:
    explicit Lexer(std::span<const vm_char_t> code) noexcept : _source(code.data(), code.size()) {}

    void setStructFieldMode(bool enabled) noexcept { _structFieldMode = enabled; }

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
            if (_structFieldMode && isStructFieldSeparator()) {
                advance();
                return Token{TokenType::Semicolon, _source.substr(start, 1), _line, startCol};
            }
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
        case ',': {
            advance();
            return Token{TokenType::Comma, _source.substr(_cursor - 1, 1), _line, startCol};
        }
        case '(': {
            advance();
            return Token{TokenType::LParen, _source.substr(_cursor - 1, 1), _line, startCol};
        }
        case ')': {
            advance();
            return Token{TokenType::RParen, _source.substr(_cursor - 1, 1), _line, startCol};
        }
        case '{': {
            advance();
            return Token{TokenType::LBrace, _source.substr(_cursor - 1, 1), _line, startCol};
        }
        case '}': {
            advance();
            return Token{TokenType::RBrace, _source.substr(_cursor - 1, 1), _line, startCol};
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
    [[nodiscard]] bool isStructFieldSeparator() const noexcept {
        const auto lineStart = _source.find_last_of("\r\n", _cursor);
        const auto prefixStart = lineStart == std::string_view::npos ? 0 : lineStart + 1;
        if (std::all_of(_source.begin() + prefixStart, _source.begin() + _cursor, [](char c) {
                return c == ' ' || c == '\t';
            })) {
            return false;
        }

        auto next = _cursor + 1;
        while (next < _source.size() && (_source[next] == ' ' || _source[next] == '\t')) {
            ++next;
        }
        if (next == _source.size() || _source[next] == '\r' || _source[next] == '\n' || _source[next] == '}') {
            return true;
        }

        while (next < _source.size() && _source[next] != ':' && _source[next] != ';' &&
               !std::isspace(static_cast<unsigned char>(_source[next])) && _source[next] != '{' &&
               _source[next] != '}' && _source[next] != ',' && _source[next] != '(' && _source[next] != ')') {
            ++next;
        }
        while (next < _source.size() && (_source[next] == ' ' || _source[next] == '\t')) {
            ++next;
        }
        return next < _source.size() && _source[next] == ':';
    }

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
        return std::isspace(c) || c == ':' || c == ';' || c == '"' || c == ',' || c == '(' || c == ')' || c == '{' ||
               c == '}';
    }

    std::string_view _source;
    uint32_t _cursor{0};
    uint32_t _line{1};
    uint32_t _column{1};
    bool _structFieldMode{false};
};

class Parser {
public:
    explicit Parser(Lexer& lexer) : _lexer(lexer) { advance(); }

    std::pair<std::vector<uint8_t>, Format> parse() {
        Advt result{};
        while (_currentToken.type != TokenType::Eof) {
            skipNewlinesAndComments();
            if (_currentToken.type == TokenType::Eof) {
                break;
            }
            if (_currentToken.type == TokenType::Section) {
                std::string_view sec = _currentToken.value;
                if (equals(sec, ".header"sv)) {
                    parseHeaderSection(result);
                } else if (equals(sec, ".export"sv)) {
                    parseExportSection(result);
                } else if (equals(sec, ".data")) {
                    parseDataSection(result);
                } else if (equals(sec, ".text")) {
                    parseTextSection(result);
                } else {
                    error(fmt::format("Unknown section '{}'", sec), TranslateResult::UnknownSection, true);
                }
            } else {
                error("Expected section header starting with '.'", TranslateResult::ExpectedSectionStartingWithDot);
            }
        }
        resolveIdentifiers(result);
        buildExportSection(result);
        return {mergeTextSection(result), result.format};
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
            error(message, TranslateResult::SyntaxError);
        }
    }

    [[noreturn]] void error(
        const std::string& message,
        TranslateResult::ErrorCode errorCode,
        bool currToken = false,
        std::optional<vm_uint32_t> line = std::nullopt) const {
        auto token = currToken ? _currentToken : _prevToken;
        throw TranslateResult{_format, errorCode, message, line.value_or(token.line), line ? 0 : token.column};
    }

    void parseHeaderSection(Advt& result) {
        advance();
        if (_currentToken.type == TokenType::Colon) {
            advance();
        }
        while (_currentToken.type != TokenType::Eof) {
            skipNewlinesAndComments();
            if (_currentToken.type == TokenType::Section || _currentToken.type == TokenType::Eof) {
                break;
            }
            expect(TokenType::Identifier, "Expected header field");
            const auto field = _currentToken.value;
            advance();
            expect(TokenType::Colon, "Expected ':' after header field");
            advance();
            expect(TokenType::Identifier, "Expected header value");
            const auto value = _currentToken.value;
            advance();
            if (_currentToken.type != TokenType::Newline && _currentToken.type != TokenType::Comment &&
                _currentToken.type != TokenType::Section && _currentToken.type != TokenType::Eof) {
                error("Unexpected token after header value", TranslateResult::SyntaxError);
            }
            if (equals(field, "format"sv)) {
                result.format = parseFormatValue(value);
            } else if (equals(field, "version"sv)) {
                result.version = parseVersionValue(value);
            } else {
                error(fmt::format("Unknown header field '{}'", field), TranslateResult::SyntaxError);
            }
        }
    }

    void parseExportSection(Advt& result) {
        advance();
        if (_currentToken.type == TokenType::Colon) {
            advance();
        }
        while (_currentToken.type != TokenType::Eof) {
            skipNewlinesAndComments();
            if (_currentToken.type == TokenType::Section || _currentToken.type == TokenType::Eof) {
                break;
            }
            expect(TokenType::Identifier, "Expected export declaration");
            const auto declaration = _currentToken.value;
            const auto line = _currentToken.line;
            advance();
            if (equals(declaration, "module"sv)) {
                if (result.moduleName) {
                    error("Duplicate module export", TranslateResult::RedefinedIdentifier, false, line);
                }
                expect(TokenType::Identifier, "Expected module name");
                result.moduleName = emplaceData(result, _currentToken.value);
                result.moduleLine = line;
                advance();
                expect(TokenType::Identifier, "Expected root frame slot count");
                result.rootFrameSlots = parseValue<vm_uint16_t>(_currentToken.value);
                advance();
                expectExportLineEnd();
                continue;
            }

            if (equals(declaration, "struct"sv)) {
                Symbol symbol{};
                symbol.kind = SymbolKind::Structure;
                symbol.line = line;
                expect(TokenType::Identifier, "Expected structure name");
                symbol.name = _currentToken.value;
                advance();
                expect(TokenType::LBrace, "Expected '{' after structure name");
                _lexer.setStructFieldMode(true);
                advance();
                std::unordered_set<std::string_view> fieldNames;
                while (_currentToken.type != TokenType::RBrace && _currentToken.type != TokenType::Eof) {
                    skipNewlinesAndComments();
                    if (_currentToken.type == TokenType::RBrace || _currentToken.type == TokenType::Eof) {
                        break;
                    }
                    SymbolParameter field{};
                    expect(TokenType::Identifier, "Expected field name");
                    const auto fieldLine = _currentToken.line;
                    field.name = _currentToken.value;
                    advance();
                    expect(TokenType::Colon, "Expected ':' after field name");
                    advance();
                    field.type = parseScriptType();
                    if (!fieldNames.insert(field.name).second) {
                        error(
                            fmt::format("Duplicate field '{}'", field.name),
                            TranslateResult::RedefinedIdentifier,
                            false,
                            fieldLine);
                    }
                    symbol.fields.push_back(field);
                    if (_currentToken.type == TokenType::Semicolon) {
                        advance();
                    } else if (
                        _currentToken.type != TokenType::Newline && _currentToken.type != TokenType::Comment &&
                        _currentToken.type != TokenType::RBrace && _currentToken.type != TokenType::Eof) {
                        error("Unexpected token after structure field", TranslateResult::SyntaxError);
                    }
                }
                expect(TokenType::RBrace, "Expected '}' after structure fields");
                advance();
                _lexer.setStructFieldMode(false);
                expectExportLineEnd();
                if (auto it = result.symbolMap.find(symbol.name); it != result.symbolMap.end()) {
                    error(
                        fmt::format("Duplicate export identifier '{}'", symbol.name),
                        TranslateResult::RedefinedIdentifier,
                        false,
                        line);
                } else {
                    result.symbolMap[symbol.name] = std::move(symbol);
                }
                continue;
            }

            Symbol symbol{};
            symbol.line = line;
            expect(TokenType::Identifier, "Expected export name");
            symbol.name = _currentToken.value;
            advance();
            if (equals(declaration, "var"sv)) {
                symbol.kind = SymbolKind::Variable;
                expect(TokenType::Colon, "Expected ':' after variable name");
                advance();
                const auto type = parseScriptType();
                symbol.type = type.code;
                symbol.structureName = type.structureName;
            } else if (equals(declaration, "native"sv) || equals(declaration, "function"sv)) {
                const bool isFunction = equals(declaration, "function"sv);
                symbol.kind = isFunction ? SymbolKind::Function : SymbolKind::Native;
                symbol.parameters = parseExportParameters();
                if (_currentToken.type == TokenType::Colon) {
                    advance();
                    const auto type = parseScriptType();
                    symbol.type = type.code;
                    symbol.structureName = type.structureName;
                }
            } else {
                error(
                    fmt::format("Unknown export declaration '{}'", declaration),
                    TranslateResult::SyntaxError,
                    false,
                    line);
            }
            expectExportLineEnd();
            if (auto it = result.symbolMap.find(symbol.name); it != result.symbolMap.end()) {
                error(
                    fmt::format("Duplicate export identifier '{}'", symbol.name),
                    TranslateResult::RedefinedIdentifier,
                    false,
                    line);
            } else {
                result.symbolMap[symbol.name] = std::move(symbol);
            }
        }
    }

    void parseDataSection(Advt& result) {
        advance();
        if (_currentToken.type == TokenType::Colon) {
            advance();
        }
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
            if (result.symbolMap.contains(name)) {
                error(
                    fmt::format("Duplicate identifier '{}'", _currentToken.value),
                    TranslateResult::RedefinedIdentifier);
            }
            Symbol symbol{};
            symbol.kind = SymbolKind::Data;
            symbol.name = name;
            symbol.type = VM_VALUE_TYPE_STRING;
            symbol.offset = emplaceData(result, _currentToken.value);
            symbol.line = _currentToken.line;
            result.symbolMap[name] = std::move(symbol);
            advance();
        }
    }

    void parseTextSection(Advt& result) {
        advance();
        if (_currentToken.type == TokenType::Colon) {
            advance();
        }
        while (_currentToken.type != TokenType::Eof) {
            skipNewlinesAndComments();
            if (_currentToken.type == TokenType::Eof ||
                (_currentToken.type == TokenType::Section && !equals(_currentToken.value, ".function"sv))) {
                break;
            }
            auto isPushOp = false;
            auto requiredOperands = 0;
            Instruction instruction{};
            instruction.line = _currentToken.line;
            if (_currentToken.type == TokenType::Section && equals(_currentToken.value, ".function"sv)) {
                advance();
                parseFunctionPrologue(instruction);
                pushInstruction(result, instruction);
                continue;
            } else if (_currentToken.type == TokenType::Identifier) {
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
                    isPushOp = equals(_currentToken.value, "push");
                    if (!isPushOp) {
                        const auto [opcode, operands] = parseOpcode(_currentToken.value);
                        instruction.opcode = opcode;
                        requiredOperands = operands;
                    }
                    advance();
                } else {
                    isPushOp = equals(firstToken, "push");
                    if (!isPushOp) {
                        const auto [opcode, operands] = parseOpcode(firstToken);
                        instruction.opcode = opcode;
                        requiredOperands = operands;
                    }
                }
            } else {
                error("Expected instruction or label", TranslateResult::ExpectedInstruction);
            }
            int operandIndex = 0;
            while (_currentToken.type != TokenType::Newline && _currentToken.type != TokenType::Comment &&
                   _currentToken.type != TokenType::Eof) {
                if (_currentToken.type == TokenType::Identifier || _currentToken.type == TokenType::String) {
                    if (operandIndex >= std::size(instruction.operands)) {
                        error("Too many operands for instruction", TranslateResult::UnexpectedTokenInOperand);
                    }
                    instruction.operands[operandIndex++] = _currentToken.value;
                } else {
                    error("Unexpected token in operands", TranslateResult::UnexpectedTokenInOperand);
                }
                advance();
            }
            if (instruction.opcode == VM_OPCODE_POP && operandIndex == 0) {
                instruction.operands[operandIndex++] = "1"sv;
            }
            if (isPushOp) {
                const auto [opcode, operands] = parsePushOpcode(instruction.operands[0]);
                instruction.opcode = opcode;
                requiredOperands = operands;
            }
            if (operandIndex != requiredOperands) {
                error(
                    fmt::format("Instruction requires {} operands, but got {}", requiredOperands, operandIndex),
                    TranslateResult::SyntaxError);
            }
            pushInstruction(result, instruction);
        }
    }

    void parseFunctionPrologue(Instruction& instruction) {
        expect(TokenType::Identifier, "Expected function label");
        instruction.opcode = VM_OPCODE_FUNCTION_PROLOGUE;
        instruction.label = _currentToken.value;
        instruction.line = _currentToken.line;
        advance();

        for (auto& operand : instruction.operands) {
            operand = "0"sv;
        }

        if (_currentToken.type == TokenType::LParen) {
            advance();
            std::array<bool, 4> seen{};
            if (_currentToken.type != TokenType::RParen) {
                while (true) {
                    expect(TokenType::Identifier, "Expected function prologue field");
                    const auto field = _currentToken.value;
                    const auto fieldLine = _currentToken.line;
                    size_t operandIndex = 0;
                    vm_uint64_t maxValue = std::numeric_limits<vm_uint8_t>::max();
                    if (equals(field, "depth"sv)) {
                        operandIndex = 0;
                    } else if (equals(field, "slots"sv)) {
                        operandIndex = 1;
                        maxValue = std::numeric_limits<vm_uint16_t>::max();
                    } else if (equals(field, "params"sv)) {
                        operandIndex = 2;
                    } else if (equals(field, "rets"sv)) {
                        operandIndex = 3;
                    } else {
                        error(fmt::format("Unknown function prologue field '{}'", field), TranslateResult::SyntaxError);
                    }
                    if (seen[operandIndex]) {
                        error(
                            fmt::format("Duplicate function prologue field '{}'", field),
                            TranslateResult::SyntaxError,
                            false,
                            fieldLine);
                    }
                    seen[operandIndex] = true;
                    advance();
                    expect(TokenType::Colon, "Expected ':' after function prologue field");
                    advance();
                    expect(TokenType::Identifier, "Expected non-negative function prologue value");
                    const auto value = _currentToken.value;
                    const auto valueLine = _currentToken.line;
                    const auto parsedValue = parseValue<vm_sint64_t>(value);
                    if (parsedValue < 0) {
                        error(
                            "Function prologue values must be non-negative",
                            TranslateResult::SyntaxError,
                            false,
                            valueLine);
                    }
                    if (static_cast<vm_uint64_t>(parsedValue) > maxValue) {
                        error(
                            fmt::format("Function prologue value out of range '{}'", value),
                            TranslateResult::ValueOutOfRange,
                            false,
                            valueLine);
                    }
                    instruction.operands[operandIndex] = value;
                    advance();
                    if (_currentToken.type == TokenType::RParen) {
                        break;
                    }
                    expect(TokenType::Comma, "Expected ',' or ')' after function prologue field");
                    advance();
                }
            }
            expect(TokenType::RParen, "Expected ')' after function prologue fields");
            advance();
        }

        expect(TokenType::Colon, "Expected ':' after function prologue");
        advance();
        if (_currentToken.type != TokenType::Newline && _currentToken.type != TokenType::Comment &&
            _currentToken.type != TokenType::Section && _currentToken.type != TokenType::Eof) {
            error("Unexpected token after function prologue", TranslateResult::SyntaxError);
        }
    }

    [[nodiscard]] Format parseFormatValue(std::string_view str) {
        if (equals(str, "ADVT"sv)) {
            _format = Format::Advt;
            return Format::Advt;
        }
        _format = Format::Unknown;
        error(fmt::format("Unknown format '{}'", str), TranslateResult::UnknownFormat);
    }

    [[nodiscard]] vm_uint32_t parseVersionValue(std::string_view strVer) {
        auto str = strVer;
        if (auto major = parseComponent(str, false)) {
            if (auto minor = parseComponent(str, false)) {
                if (auto micro = parseComponent(str, true)) {
                    auto version = VM_VERSION_ENCODE(major.value(), minor.value(), micro.value());
                    if ((version >> 24) == (VM_VERSION >> 24)) {
                        return version;
                    }
                }
            }
        }
        error(
            fmt::format("Script version '{}' is incompatible with adv version '{}'", strVer, VM_VERSION_STRING),
            TranslateResult::IncompatibleVersion);
    }

    [[nodiscard]] SymbolType parseScriptType() {
        expect(TokenType::Identifier, "Expected export type");
        const auto typeName = _currentToken.value;
        advance();
        if (equals(typeName, "bool"sv)) {
            return {VM_VALUE_TYPE_BOOL, {}};
        }
        if (equals(typeName, "int"sv)) {
            return {VM_VALUE_TYPE_INT, {}};
        }
        if (equals(typeName, "float"sv)) {
            return {VM_VALUE_TYPE_FLOAT, {}};
        }
        if (equals(typeName, "string"sv)) {
            return {VM_VALUE_TYPE_STRING, {}};
        }
        return {utils::ExportTypeStructure, typeName};
    }

    [[nodiscard]] std::vector<SymbolParameter> parseExportParameters() {
        expect(
            _currentToken.type == TokenType::LBrace ? TokenType::LBrace : TokenType::LParen,
            "Expected '(' before function parameters");
        const auto closeType = _currentToken.type == TokenType::LBrace ? TokenType::RBrace : TokenType::RParen;
        advance();
        std::vector<SymbolParameter> parameters;
        if (_currentToken.type == closeType) {
            advance();
            return parameters;
        }
        while (true) {
            expect(TokenType::Identifier, "Expected parameter name");
            const auto name = _currentToken.value;
            advance();
            expect(TokenType::Colon, "Expected ':' after parameter name");
            advance();
            const auto type = parseScriptType();
            parameters.push_back({name, type});
            if (_currentToken.type == closeType) {
                advance();
                return parameters;
            }
            expect(TokenType::Comma, "Expected ',' or closing parenthesis after parameter");
            advance();
        }
    }

    void expectExportLineEnd() {
        if (_currentToken.type != TokenType::Newline && _currentToken.type != TokenType::Comment &&
            _currentToken.type != TokenType::Section && _currentToken.type != TokenType::Eof) {
            error("Unexpected token after export declaration", TranslateResult::SyntaxError);
        }
    }

    template <typename T>
    [[nodiscard]] T parseValue(std::string_view str) const {
        static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);
        T val{};
        const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.length(), val);
        if (ec == std::errc() && ptr == str.data() + str.length()) {
            return val;
        } else if (ec == std::errc::result_out_of_range) {
            error(fmt::format("Value out of range '{}'", str), TranslateResult::ValueOutOfRange);
        } else {
            error(fmt::format("Syntax Error '{}'", str), TranslateResult::SyntaxError);
        }
    }

    void pushInstruction(Advt& result, const Instruction& instruction) {
        const auto label = instruction.label;
        const auto opcode = instruction.opcode;
        const auto operand = instruction.operands[0];
        if (!label.empty()) {
            if (result.labelMap.contains(label)) {
                error(fmt::format("Duplicate label identifier '{}'", label), TranslateResult::RedefinedIdentifier);
            }
            result.labelMap[label] = pollOffset(result.textSection);
        }
        result.textSection.push_back((uint8_t)opcode);
        switch (instruction.opcode) {
        case VM_OPCODE_PUSH_LOCAL:
        case VM_OPCODE_POP_LOCAL:
        case VM_OPCODE_POP:
            pushUint16(parseValue<vm_uint16_t>(operand), result.textSection);
            break;
        case VM_OPCODE_PUSH_OUTER_LOCAL:
        case VM_OPCODE_POP_OUTER_LOCAL:
            result.textSection.push_back(parseValue<vm_uint8_t>(operand));
            pushUint16(parseValue<vm_uint16_t>(instruction.operands[1]), result.textSection);
            break;
        case VM_OPCODE_JUMP:
        case VM_OPCODE_JUMP_IF_FALSE:
            result.jumps.emplace_back(operand, pollOffset(result.textSection), instruction.line);
            pushUint32(0, result.textSection);
            break;
        case VM_OPCODE_PUSH_INT:
            pushInt32(parseValue<vm_sint32_t>(operand), result.textSection);
            break;
        case VM_OPCODE_PUSH_FLOAT:
            pushFloat(parseValue<vm_float32_t>(operand), result.textSection);
            break;
        case VM_OPCODE_PUSH_STRING_FROM_DATA:
            if (!operand.empty() && operand[0] == '"') {
                pushUint32(emplaceData(result, operand), result.textSection);
            } else {
                if (auto it = result.symbolMap.find(operand);
                    it != result.symbolMap.end() && it->second.kind == SymbolKind::Data) {
                    pushUint32(it->second.offset, result.textSection);
                } else {
                    result.pushData.emplace_back(operand, pollOffset(result.textSection), instruction.line);
                    pushUint32(0, result.textSection);
                }
            }
            break;
        case VM_OPCODE_IMPORT:
            pushUint32(emplaceData(result, operand), result.textSection);
            break;
        case VM_OPCODE_PUSH_GLOBAL_BY_NAME:
        case VM_OPCODE_POP_GLOBAL_BY_NAME:
            pushUint32(emplaceData(result, operand), result.textSection);
            break;
        case VM_OPCODE_FUNCTION_PROLOGUE:
            result.textSection.push_back(parseValue<vm_uint8_t>(instruction.operands[0]));
            pushUint16(parseValue<vm_uint16_t>(instruction.operands[1]), result.textSection);
            result.textSection.push_back(parseValue<vm_uint8_t>(instruction.operands[2]));
            result.textSection.push_back(parseValue<vm_uint8_t>(instruction.operands[3]));
            break;
        case VM_OPCODE_CALL_NATIVE_BY_NAME:
        case VM_OPCODE_CALL_BY_NAME:
            result.calls.emplace_back(operand, pollOffset(result.textSection), instruction.line);
            pushUint32(emplaceData(result, operand), result.textSection);
            break;
        case VM_OPCODE_CALL:
            result.calls.emplace_back(operand, pollOffset(result.textSection), instruction.line);
            pushUint32(0, result.textSection);
            result.textSection.push_back(parseValue<vm_uint8_t>(instruction.operands[1]));
            pushUint16(parseValue<vm_uint16_t>(instruction.operands[2]), result.textSection);
            break;
        default:
            break;
        }
    }

    [[nodiscard]] vm_uint32_t emplaceData(Advt& result, std::string_view str) {
        if (auto it = result.dataMap.find(str); it != result.dataMap.end()) {
            return it->second;
        } else {
            const auto offset = pushStringData(str, result.dataSection);
            result.dataMap[str] = offset;
            return offset;
        }
    }

    void resolveIdentifiers(Advt& result) {
        for (const auto& patch : result.pushData) {
            if (auto it = result.symbolMap.find(patch.identifier);
                it != result.symbolMap.end() && it->second.kind == SymbolKind::Data) {
                patchUint32(patch.offset, it->second.offset, result.textSection);
            }
        }
        for (const auto& [id, offsetText, line] : result.jumps) {
            auto it = result.labelMap.find(id);
            if (it == result.labelMap.end()) {
                error(
                    fmt::format("Label identifier '{}' not found", id),
                    TranslateResult::UnknownIdentifier,
                    false,
                    line);
            }
            const auto offset = it->second;
            patchUint32(offsetText, offset, result.textSection);
        }
        for (auto& [id, symbol] : result.symbolMap) {
            if (symbol.kind == SymbolKind::Function) {
                if (auto lb = result.labelMap.find(id); lb != result.labelMap.end()) {
                    symbol.offset = lb->second;
                } else {
                    error(
                        fmt::format("Function label '{}' not found", id),
                        TranslateResult::InvalidIdentifierReference,
                        false,
                        symbol.line);
                }
            }
        }
        for (const auto& [id, offsetText, line] : result.calls) {
            const auto opcode = result.textSection[offsetText - 1];
            if (!result.symbolMap.contains(id)) {
                if (auto it = result.labelMap.find(id); it != result.labelMap.end()) {
                    result.textSection[offsetText - 1] = VM_OPCODE_CALL;
                    patchUint32(offsetText, it->second, result.textSection);
                } else if (opcode != VM_OPCODE_CALL_BY_NAME && opcode != VM_OPCODE_CALL_NATIVE_BY_NAME) {
                    error(
                        fmt::format("Identifier '{}' not found", id), TranslateResult::UnknownIdentifier, false, line);
                }
            }
        }
    }

    [[nodiscard]] vm_uint16_t checkedUint16(size_t value, vm_uint32_t line) {
        if (value > std::numeric_limits<vm_uint16_t>::max()) {
            error("Export section value exceeds 16-bit limit", TranslateResult::ValueOutOfRange, false, line);
        }
        return static_cast<vm_uint16_t>(value);
    }

    [[nodiscard]] vm_uint8_t checkedUint8(size_t value, vm_uint32_t line) {
        if (value > std::numeric_limits<vm_uint8_t>::max()) {
            error("Export parameter count exceeds 8-bit limit", TranslateResult::ValueOutOfRange, false, line);
        }
        return static_cast<vm_uint8_t>(value);
    }

    void buildExportSection(Advt& result) {
        if (result.symbolMap.empty() && !result.moduleName.has_value()) {
            return;
        }
        vm_script_export_t exports{};
        exports.module_name = result.moduleName.value_or(0);
        exports.root_frame_slots = result.rootFrameSlots;
        result.exportSection.resize(sizeof(exports));
        auto appendRecord = [&](auto record) {
            record = utils::swapBytes(record);
            const auto* data = reinterpret_cast<const uint8_t*>(&record);
            result.exportSection.insert(result.exportSection.end(), data, data + sizeof(record));
        };
        auto encodeType = [&](const SymbolType& type, vm_uint32_t line) {
            if (type.code > VM_VALUE_TYPE_STRING) {
                const auto dataOffset = emplaceData(result, type.structureName);
                if (dataOffset > (std::numeric_limits<vm_uint32_t>::max() >> 3)) {
                    error(
                        "Structure type name offset exceeds export type limit",
                        TranslateResult::ValueOutOfRange,
                        false,
                        line);
                }
                return utils::encodeExportStructureType(dataOffset);
            }
            return type.code;
        };
        for (const auto& [_, symbol] : result.symbolMap) {
            if (symbol.kind != SymbolKind::Structure) {
                continue;
            }
            const auto structLine = symbol.line;
            appendRecord(
                vm_script_export_struct_t{
                    emplaceData(result, symbol.name), checkedUint16(symbol.fields.size(), structLine)});
            exports.struct_count = checkedUint16(static_cast<size_t>(exports.struct_count) + 1, structLine);
            exports.struct_size =
                checkedUint16(static_cast<size_t>(exports.struct_size) + sizeof(vm_script_export_struct_t), structLine);
            for (const auto& field : symbol.fields) {
                const auto type = encodeType(field.type, structLine);
                appendRecord(vm_script_export_field_t{emplaceData(result, field.name), type});
                exports.struct_size = checkedUint16(
                    static_cast<size_t>(exports.struct_size) + sizeof(vm_script_export_field_t), structLine);
            }
        }
        for (const auto& [_, symbol] : result.symbolMap) {
            if (symbol.kind == SymbolKind::Variable) {
                const SymbolType type{symbol.type, symbol.structureName};
                appendRecord(vm_script_export_var_t{emplaceData(result, symbol.name), encodeType(type, symbol.line)});
                exports.var_count = checkedUint16(static_cast<size_t>(exports.var_count) + 1, symbol.line);
                exports.var_size =
                    checkedUint16(static_cast<size_t>(exports.var_size) + sizeof(vm_script_export_var_t), symbol.line);
            }
        }
        for (const auto& [_, symbol] : result.symbolMap) {
            if (symbol.kind == SymbolKind::Function) {
                const SymbolType type{symbol.type, symbol.structureName};
                vm_script_export_func_t func{
                    emplaceData(result, symbol.name), symbol.offset, encodeType(type, symbol.line)};
                func.param_count = checkedUint8(symbol.parameters.size(), symbol.line);
                appendRecord(func);
                exports.func_count = checkedUint16(static_cast<size_t>(exports.func_count) + 1, symbol.line);
                exports.func_size = checkedUint16(static_cast<size_t>(exports.func_size) + sizeof(func), symbol.line);
                for (const auto& param : symbol.parameters) {
                    appendRecord(
                        vm_script_export_param_t{emplaceData(result, param.name), encodeType(param.type, symbol.line)});
                    exports.func_size = checkedUint16(
                        static_cast<size_t>(exports.func_size) + sizeof(vm_script_export_param_t), symbol.line);
                }
            }
        }
        for (const auto& [_, symbol] : result.symbolMap) {
            if (symbol.kind == SymbolKind::Native) {
                const SymbolType returnType{symbol.type, symbol.structureName};
                vm_script_export_native_t native{emplaceData(result, symbol.name), encodeType(returnType, symbol.line)};
                native.param_count = checkedUint8(symbol.parameters.size(), symbol.line);
                appendRecord(native);
                exports.native_count = checkedUint16(static_cast<size_t>(exports.native_count) + 1, symbol.line);
                exports.native_size =
                    checkedUint16(static_cast<size_t>(exports.native_size) + sizeof(native), symbol.line);
                for (const auto& param : symbol.parameters) {
                    appendRecord(
                        vm_script_export_param_t{emplaceData(result, param.name), encodeType(param.type, symbol.line)});
                    exports.native_size = checkedUint16(
                        static_cast<size_t>(exports.native_size) + sizeof(vm_script_export_param_t), symbol.line);
                }
            }
        }
        exports = utils::swapBytes(exports);
        std::memcpy(result.exportSection.data(), &exports, sizeof(exports));
    }

    std::vector<vm_uint8_t> mergeTextSection(Advt& result) {
        std::vector<vm_uint8_t> bytecode(Advt::offsetExport);
        const auto offsetExport = static_cast<vm_uint32_t>(bytecode.size());
        bytecode.insert(bytecode.end(), result.exportSection.begin(), result.exportSection.end());
        const auto offsetData = static_cast<vm_uint32_t>(bytecode.size());
        bytecode.insert(bytecode.end(), result.dataSection.begin(), result.dataSection.end());
        const auto offsetText = static_cast<vm_uint32_t>(bytecode.size());
        bytecode.insert(bytecode.end(), result.textSection.begin(), result.textSection.end());
        auto header = aliasCast<vm_script_header_t*>(bytecode.data());
        auto table = aliasCast<vm_script_table_t*>(bytecode.data() + sizeof(vm_script_header_t));

        header->magic[0] = 'a';
        header->magic[1] = 'd';
        header->magic[2] = 'v';
        header->magic[3] = 'b';
        header->checksum = 0;
        header->version = swapBytes(static_cast<vm_uint32_t>(VM_VERSION));
        table->export_offset = swapBytes(offsetExport);
        table->export_size = swapBytes(static_cast<vm_uint32_t>(result.exportSection.size()));
        table->data_offset = swapBytes(offsetData);
        table->data_size = swapBytes(static_cast<vm_uint32_t>(result.dataSection.size()));
        table->text_offset = swapBytes(static_cast<vm_uint32_t>(offsetText));
        table->text_size = swapBytes(static_cast<vm_uint32_t>(result.textSection.size()));
        auto view = std::views::all(bytecode);
        header->checksum = swapBytes(adler32(std::move(view)));
        return bytecode;
    }

    [[nodiscard]] std::pair<vm_opcode_t, int> parseOpcode(std::string_view str) const {
        if (equals(str, "import"sv)) {
            return {VM_OPCODE_IMPORT, 1};
        }
        if (equals(str, "push"sv)) {
            assert(!"unreachable code");
            return {VM_OPCODE_NO_OPERATION, 0};
        }
        if (equals(str, "push_global"sv)) {
            return {VM_OPCODE_PUSH_GLOBAL_BY_NAME, 1};
        }
        if (equals(str, "pop_global"sv)) {
            return {VM_OPCODE_POP_GLOBAL_BY_NAME, 1};
        }
        if (equals(str, "push_local"sv)) {
            return {VM_OPCODE_PUSH_LOCAL, 1};
        }
        if (equals(str, "pop_local"sv)) {
            return {VM_OPCODE_POP_LOCAL, 1};
        }
        if (equals(str, "push_outer"sv)) {
            return {VM_OPCODE_PUSH_OUTER_LOCAL, 2};
        }
        if (equals(str, "pop_outer"sv)) {
            return {VM_OPCODE_POP_OUTER_LOCAL, 2};
        }
        if (equals(str, "pop"sv)) {
            return {VM_OPCODE_POP, 1};
        }
        if (equals(str, "btoi"sv)) {
            return {VM_OPCODE_BOOL_TO_INT, 0};
        }
        if (equals(str, "btof"sv)) {
            return {VM_OPCODE_BOOL_TO_FLOAT, 0};
        }
        if (equals(str, "btos"sv)) {
            return {VM_OPCODE_BOOL_TO_STRING, 0};
        }
        if (equals(str, "itob"sv)) {
            return {VM_OPCODE_INT_TO_BOOL, 0};
        }
        if (equals(str, "itof"sv)) {
            return {VM_OPCODE_INT_TO_FLOAT, 0};
        }
        if (equals(str, "itos"sv)) {
            return {VM_OPCODE_INT_TO_STRING, 0};
        }
        if (equals(str, "ftob"sv)) {
            return {VM_OPCODE_FLOAT_TO_BOOL, 0};
        }
        if (equals(str, "ftoi"sv)) {
            return {VM_OPCODE_FLOAT_TO_INT, 0};
        }
        if (equals(str, "ftos"sv)) {
            return {VM_OPCODE_FLOAT_TO_STRING, 0};
        }
        if (equals(str, "stob"sv)) {
            return {VM_OPCODE_STRING_TO_BOOL, 0};
        }
        if (equals(str, "stoi"sv)) {
            return {VM_OPCODE_STRING_TO_INT, 0};
        }
        if (equals(str, "stof"sv)) {
            return {VM_OPCODE_STRING_TO_FLOAT, 0};
        }
        if (equals(str, "bits_not"sv)) {
            return {VM_OPCODE_BITWISE_NOT, 0};
        }
        if (equals(str, "logic_not"sv)) {
            return {VM_OPCODE_LOGICAL_NOT, 0};
        }
        if (equals(str, "neg"sv)) {
            return {VM_OPCODE_NUMERIC_NEGATION, 0};
        }
        if (equals(str, "and"sv)) {
            return {VM_OPCODE_BITWISE_AND, 0};
        }
        if (equals(str, "or"sv)) {
            return {VM_OPCODE_BITWISE_OR, 0};
        }
        if (equals(str, "xor"sv)) {
            return {VM_OPCODE_BITWISE_XOR, 0};
        }
        if (equals(str, "lsl"sv)) {
            return {VM_OPCODE_LOGIC_SHIFT_LEFT, 0};
        }
        if (equals(str, "lsr"sv)) {
            return {VM_OPCODE_LOGIC_SHIFT_RIGHT, 0};
        }
        if (equals(str, "asl"sv)) {
            return {VM_OPCODE_ARITHMETIC_SHIFT_LEFT, 0};
        }
        if (equals(str, "asr"sv)) {
            return {VM_OPCODE_ARITHMETIC_SHIFT_RIGHT, 0};
        }
        if (equals(str, "add"sv)) {
            return {VM_OPCODE_ADD, 0};
        }
        if (equals(str, "sub"sv)) {
            return {VM_OPCODE_SUBTRACT, 0};
        }
        if (equals(str, "mul"sv)) {
            return {VM_OPCODE_MULTIPLY, 0};
        }
        if (equals(str, "div"sv)) {
            return {VM_OPCODE_DIVISION, 0};
        }
        if (equals(str, "mod"sv)) {
            return {VM_OPCODE_MOD, 0};
        }
        if (equals(str, "eq"sv)) {
            return {VM_OPCODE_EQUAL, 0};
        }
        if (equals(str, "ne"sv)) {
            return {VM_OPCODE_NOT_EQUAL, 0};
        }
        if (equals(str, "less"sv)) {
            return {VM_OPCODE_LESS, 0};
        }
        if (equals(str, "less_equal"sv)) {
            return {VM_OPCODE_LESS_EQUAL, 0};
        }
        if (equals(str, "great"sv)) {
            return {VM_OPCODE_GREAT, 0};
        }
        if (equals(str, "great_equal"sv)) {
            return {VM_OPCODE_GREAT_EQUAL, 0};
        }
        if (equals(str, "call"sv)) {
            return {VM_OPCODE_CALL_BY_NAME, 1};
        }
        if (equals(str, "call_native"sv)) {
            return {VM_OPCODE_CALL_NATIVE_BY_NAME, 1};
        }
        if (equals(str, "ret"sv)) {
            return {VM_OPCODE_RETURN, 0};
        }
        if (equals(str, "jump"sv)) {
            return {VM_OPCODE_JUMP, 1};
        }
        if (equals(str, "jfalse"sv)) {
            return {VM_OPCODE_JUMP_IF_FALSE, 1};
        }
        if (equals(str, "yield"sv)) {
            return {VM_OPCODE_YIELD, 0};
        }
        if (equals(str, "wait"sv)) {
            return {VM_OPCODE_WAIT, 0};
        }
        if (equals(str, "halt"sv)) {
            return {VM_OPCODE_HALT, 0};
        }
        error(fmt::format("Unknown opcode '{}'", str), TranslateResult::UnknownOpcode);
    }

    [[nodiscard]] std::pair<vm_opcode_t, int> parsePushOpcode(std::string_view operand) const noexcept {
        if (!operand.empty() && operand.front() == '"') {
            return {VM_OPCODE_PUSH_STRING_FROM_DATA, 1};
        }
        if (equals(operand, "true"sv)) {
            return {VM_OPCODE_PUSH_TRUE, 0};
        }
        if (equals(operand, "false"sv)) {
            return {VM_OPCODE_PUSH_FALSE, 0};
        }
        if (const auto pos = operand.find('.'); pos != std::string_view::npos) {
            vm_float32_t floatValue{};
            if (const auto [_, ec] = std::from_chars(operand.data(), operand.data() + operand.size(), floatValue);
                ec == std::errc{}) {
                return {VM_OPCODE_PUSH_FLOAT, 1};
            }
        }
        vm_sint32_t intValue{};
        if (const auto [_, ec] = std::from_chars(operand.data(), operand.data() + operand.size(), intValue);
            ec == std::errc{}) {
            return {VM_OPCODE_PUSH_INT, 1};
        }
        return {VM_OPCODE_PUSH_STRING_FROM_DATA, 1};
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
        val = swapBytes(val);
        buffer.push_back(static_cast<uint8_t>((val >> 0) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    }

    static void pushUint32(vm_uint32_t val, std::vector<uint8_t>& buffer) {
        val = swapBytes(val);
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
        val = swapBytes(val);
        buffer[offset + 0] = static_cast<uint8_t>((val >> 0) & 0xFF);
        buffer[offset + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
        buffer[offset + 2] = static_cast<uint8_t>((val >> 16) & 0xFF);
        buffer[offset + 3] = static_cast<uint8_t>((val >> 24) & 0xFF);
    }

    [[nodiscard]] static vm_uint32_t pushStringData(std::string_view str, std::vector<uint8_t>& buffer) {
        const auto offset = static_cast<vm_uint32_t>(buffer.size());
        const auto isQuoted = str.size() >= 2 && str.front() == '"' && str.back() == '"';
        const auto content = isQuoted ? str.substr(1, str.size() - 2) : str;
        pushUint16(static_cast<vm_uint16_t>(content.length()), buffer);
        buffer.insert(buffer.end(), content.begin(), content.end());
        buffer.push_back(uint8_t('\0'));
        return offset;
    }

    [[nodiscard]] static vm_uint32_t pollOffset(std::vector<uint8_t>& buffer) noexcept {
        return static_cast<vm_uint32_t>(buffer.size());
    }

    Lexer& _lexer;
    Token _prevToken;
    Token _currentToken;
    Format _format;
};

} // namespace

bool isBinaryCode(std::span<const char> code, bool checkSize) noexcept {
    constexpr auto headerSize = sizeof(vm_script_header_t) + sizeof(vm_script_table_t);

    if (code.size() < headerSize) {
        return false;
    }

    const auto magic = aliasCast<vm_script_header_t*>(code.data())->magic;
    if (magic[0] != 'a' || magic[1] != 'd' || magic[2] != 'v' || magic[3] != 'b') {
        return false;
    }

    const auto table = utils::read<vm_script_table_t>({(const vm_uint8_t*)code.data(), code.size()});
    const auto exportLeft = table.export_offset;
    const auto exportRight = exportLeft + table.export_size;
    const auto dataLeft = table.data_offset;
    const auto dataRight = dataLeft + table.data_size;
    const auto textLeft = table.text_offset;
    const auto textRight = textLeft + table.text_size;

    if (exportLeft < headerSize || dataLeft < headerSize || textLeft < headerSize) {
        return false;
    }
    if (checkSize && (exportRight > code.size() || dataRight > code.size() || textRight > code.size())) {
        return false;
    }
    return dataRight <= textLeft || textRight <= dataLeft;
}

bool validateChecksum(std::span<const char> code, uint32_t* actual, uint32_t* expected) noexcept {
    if (code.size() < sizeof(vm_script_header_t)) {
        return false;
    }
    decltype(vm_script_header_t::checksum) sum = 0;
    constexpr auto offsetChecksum = offsetof(vm_script_header_t, checksum);
    constexpr auto sizeofChecksum = sizeof(vm_script_header_t::checksum);
    static_assert(sizeof(sum) == sizeofChecksum, "checksum size mismatch");
    auto checksum = swapBytes(aliasCast<const vm_script_header_t*>(code.data())->checksum);
    auto block0 = code.subspan(0, offsetChecksum);
    auto block1 = std::span{(const vm_char_t*)&sum, sizeof(sum)};
    auto block2 = code.subspan(offsetChecksum + sizeofChecksum);
    auto view = std::array<std::span<const vm_char_t>, 3>{block0, block1, block2} | std::views::join;
    auto result = adler32(std::move(view));
    if (actual) {
        *actual = result;
    }
    if (expected) {
        *expected = checksum;
    }
    return result == checksum;
}

bool validateVersion(std::span<const char> code, uint32_t expectedVersion, uint32_t* compiledVersion) noexcept {
    if (code.size() < sizeof(vm_script_header_t)) {
        return false;
    }
    auto version = swapBytes(aliasCast<const vm_script_header_t*>(code.data())->version);
    if (compiledVersion) {
        *compiledVersion = version;
    }
    return (version >> 24) == (expectedVersion >> 24);
}

std::vector<uint8_t> translateTextToBinary(std::span<const char> text, TranslateResult& error) noexcept {
    try {
        error = {};
        Lexer lexer(text);
        Parser parser(lexer);
        const auto [bytecode, format] = parser.parse();
        error.format = format;
        return std::move(bytecode);
    } catch (const TranslateResult& err) {
        error = err;
    } catch (const std::bad_alloc&) {
        error = {Format::Unknown, TranslateResult::OutOfMemory, "Out of memory"};
    } catch (...) {
        error = {Format::Unknown, TranslateResult::UnknownError, "Unknown error"};
    }
    return {};
}

} // namespace utils
