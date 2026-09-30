#include "generator.hpp"
#include "context.hpp"
#include "parser.hpp"
#include "visitor.hpp"

using namespace std::literals;

namespace adv {

namespace {

class Emitter {
public:
    Emitter(Context& ctx, std::vector<uint8_t>& data, std::vector<uint8_t>& code) noexcept
        : ctx(ctx), data(data), code(code) {}

    void emitByte(uint8_t byte) { code.push_back(byte); }
    void emitUint16(adv_uint16_t val) { emitUint16(val, code); }
    void emitUint32(adv_uint32_t val) { emitUint32(val, code); }
    void emitInt32(adv_sint32_t val) { emitUint32(static_cast<adv_uint32_t>(val), code); }
    void emitFloat(adv_float32_t val) {
        static_assert(sizeof(adv_float32_t) == sizeof(adv_uint32_t));
        adv_uint32_t bits;
        std::memcpy(&bits, &val, sizeof(adv_float32_t));
        emitUint32(bits);
    }

    void emitString(vm_string_h str) {
        if (auto it = strings.find(str); it != strings.end()) {
            emitUint32(it->second, code);
        } else {
            adv_uint32_t offset = static_cast<adv_uint32_t>(data.size());
            auto view = ctx.getString(str);
            emitUint16(static_cast<adv_uint16_t>(view.length()), data);
            data.insert(data.end(), view.begin(), view.end());
            data.push_back(uint8_t('\0'));
            emitUint32(offset, code);
            strings[str] = offset;
        }
    }

    [[nodiscard]] adv_uint32_t emitJump(vm_opcode_t op) {
        emitByte(op);
        const auto patchOffset = offset();
        emitUint32(0, code);
        return patchOffset;
    }

    void patchJump(adv_uint32_t patchOffset) {
        auto targetAddress = offset();
        targetAddress = std::endian::native == std::endian::little ? targetAddress : utils::swapBytes(targetAddress);
        code[patchOffset + 0] = static_cast<uint8_t>((targetAddress >> 0) & 0xFF);
        code[patchOffset + 1] = static_cast<uint8_t>((targetAddress >> 8) & 0xFF);
        code[patchOffset + 2] = static_cast<uint8_t>((targetAddress >> 16) & 0xFF);
        code[patchOffset + 3] = static_cast<uint8_t>((targetAddress >> 24) & 0xFF);
    }

    void emitCast(vm_value_type_t from, vm_value_type_t to) {
        assert(from != to);
        switch (from) {
        case VM_VALUE_TYPE_BOOL:
            switch (to) {
            case VM_VALUE_TYPE_INT:
                emitByte(VM_OPCODE_BOOL_TO_INT);
                break;
            case VM_VALUE_TYPE_FLOAT:
                emitByte(VM_OPCODE_BOOL_TO_FLOAT);
                break;
            case VM_VALUE_TYPE_STRING:
                emitByte(VM_OPCODE_BOOL_TO_STRING);
                break;
            default:
                assert(!"unreachable code");
                break;
            }
            break;
        case VM_VALUE_TYPE_INT:
            switch (to) {
            case VM_VALUE_TYPE_BOOL:
                emitByte(VM_OPCODE_INT_TO_BOOL);
                break;
            case VM_VALUE_TYPE_FLOAT:
                emitByte(VM_OPCODE_INT_TO_FLOAT);
                break;
            case VM_VALUE_TYPE_STRING:
                emitByte(VM_OPCODE_INT_TO_STRING);
                break;
            default:
                assert(!"unreachable code");
                break;
            }
            break;
        case VM_VALUE_TYPE_FLOAT:
            switch (to) {
            case VM_VALUE_TYPE_BOOL:
                emitByte(VM_OPCODE_FLOAT_TO_BOOL);
                break;
            case VM_VALUE_TYPE_INT:
                emitByte(VM_OPCODE_FLOAT_TO_INT);
                break;
            case VM_VALUE_TYPE_STRING:
                emitByte(VM_OPCODE_FLOAT_TO_STRING);
                break;
            default:
                assert(!"unreachable code");
                break;
            }
            break;
        case VM_VALUE_TYPE_STRING:
            switch (to) {
            case VM_VALUE_TYPE_BOOL:
                emitByte(VM_OPCODE_STRING_TO_BOOL);
                break;
            case VM_VALUE_TYPE_INT:
                emitByte(VM_OPCODE_STRING_TO_INT);
                break;
            case VM_VALUE_TYPE_FLOAT:
                emitByte(VM_OPCODE_STRING_TO_FLOAT);
                break;
            default:
                assert(!"unreachable code");
                break;
            }
            break;
        default:
            assert(!"unreachable code");
            break;
        }
    }

    [[nodiscard]] adv_uint32_t offset() const noexcept { return static_cast<adv_uint32_t>(code.size()); }

    Context& ctx;
    std::vector<uint8_t>& data;
    std::vector<uint8_t>& code;
    std::unordered_map<vm_string_h, adv_uint32_t> strings;

private:
    static void emitUint16(adv_uint16_t val, std::vector<uint8_t>& buffer) {
        val = std::endian::native == std::endian::little ? val : utils::swapBytes(val);
        buffer.push_back(static_cast<uint8_t>((val >> 0) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    }

    static void emitUint32(adv_uint32_t val, std::vector<uint8_t>& buffer) {
        val = std::endian::native == std::endian::little ? val : utils::swapBytes(val);
        buffer.push_back(static_cast<uint8_t>((val >> 0) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    }
};

struct Reader {
    static adv_uint16_t readUint16(const uint8_t* code, size_t& ip) {
        const adv_uint16_t b0 = static_cast<adv_uint16_t>(code[ip + 0]) << 0;
        const adv_uint16_t b1 = static_cast<adv_uint16_t>(code[ip + 1]) << 8;
        const adv_uint16_t val = b0 | b1;
        ip += 2;
        return std::endian::native == std::endian::little ? val : utils::swapBytes(val);
    }

    static adv_uint32_t readUint32(const uint8_t* code, size_t& ip) {
        const adv_uint32_t b0 = static_cast<adv_uint32_t>(code[ip + 0]) << 0;
        const adv_uint32_t b1 = static_cast<adv_uint32_t>(code[ip + 1]) << 8;
        const adv_uint32_t b2 = static_cast<adv_uint32_t>(code[ip + 2]) << 16;
        const adv_uint32_t b3 = static_cast<adv_uint32_t>(code[ip + 3]) << 24;
        const adv_uint32_t val = b0 | b1 | b2 | b3;
        ip += 4;
        return std::endian::native == std::endian::little ? val : utils::swapBytes(val);
    }

    static adv_sint16_t readInt16(const uint8_t* code, size_t& ip) {
        return static_cast<adv_sint16_t>(readUint16(code, ip));
    }

    static adv_sint32_t readInt32(const uint8_t* code, size_t& ip) {
        return static_cast<adv_sint32_t>(readUint32(code, ip));
    }

    static adv_float32_t readFloat(const uint8_t* code, size_t& ip) {
        static_assert(sizeof(adv_float32_t) == sizeof(adv_sint32_t));
        auto bits = readInt32(code, ip);
        adv_float32_t val;
        std::memcpy(&val, &bits, sizeof(adv_float32_t));
        return val;
    }

    static std::pair<adv_uint32_t, std::string_view> readString(const uint8_t* code, size_t& ip, const uint8_t* data) {
        auto dataOffset = readUint32(code, ip);
        auto strOffset = static_cast<size_t>(dataOffset);
        const auto strLen = readUint16(data, strOffset);
        return {dataOffset, std::string_view((const char*)data + strOffset, strLen)};
    }
};

struct CodeGenVisitor {
    CodeGenVisitor(const Options& options, Context& ctx, Emitter& emitter) noexcept
        : options(options), ctx(ctx), emitter(emitter) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_PROGRAM>) {
        auto stmt = node->stmts.first_stmt;
        while (stmt) {
            acceptNode(stmt);
            stmt = stmt->next_sibling;
        }
        emitter.emitByte(VM_OPCODE_HALT);
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_BLOCK>) {
        auto stmt = node->stmts.first_stmt;
        while (stmt) {
            acceptNode(stmt);
            stmt = stmt->next_sibling;
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_VAR_DECL>) {
        if (node->var_decl.init_expr) {
            acceptNode(node->var_decl.init_expr);
        } else {
            emitter.emitByte(VM_OPCODE_PUSH_ZERO);
        }
        auto& symbolTable = ctx.symbolTable();
        symbolTable.setScope(node->var_decl.scope);
        auto sym = symbolTable.lookup(options, ctx, node->location, node->var_decl.name);
        if (sym->isGlobal) {
            emitter.emitByte(VM_OPCODE_POP_GLOBAL_BY_NAME);
            emitter.emitString(sym->name);
        } else {
            emitter.emitByte(VM_OPCODE_POP_LOCAL);
            emitter.emitUint16(sym->slotIndex);
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_FUNCTION_DECL>) {
        const auto jumpOverBody = emitter.emitJump(VM_OPCODE_JUMP);
        const auto entryIp = static_cast<uint32_t>(emitter.offset());

        acceptNode(node->func_decl.body);
        emitter.emitByte(VM_OPCODE_PUSH_ZERO);
        emitter.emitByte(VM_OPCODE_RETURN);
        emitter.patchJump(jumpOverBody);

        auto& symbolTable = ctx.symbolTable();
        symbolTable.setScope(node->func_decl.scope);
        auto sym = symbolTable.lookup(options, ctx, node->location, node->func_decl.name);

        // emitter.emitOpcode(VM_OPCODE_REGISTER_FUNCTION);
        // emitter.emitString(sym->name);
        // emitter.emitUint32(entryIp);
        // emitter.emitUint16(static_cast<uint16_t>(sym->frameSlotCount)); // Сколько слотов выделить под CallFrame!
        // emitter.emitUint8(static_cast<uint8_t>(sym->params.size()));
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_PARAM_DECL>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_IMPORT_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_IF_STMT>) {
        acceptNode(node->if_stmt.cond);

        const auto elseJumpPos = emitter.emitJump(VM_OPCODE_JUMP_IF_FALSE);
        acceptNode(node->if_stmt.then_branch);

        if (node->if_stmt.else_branch) {
            const auto endJumpPos = emitter.emitJump(VM_OPCODE_JUMP);
            emitter.patchJump(elseJumpPos);
            acceptNode(node->if_stmt.else_branch);
            emitter.patchJump(endJumpPos);
        } else {
            emitter.patchJump(elseJumpPos);
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_WHILE_STMT>) {
        const auto loopStartIp = static_cast<uint32_t>(emitter.offset());

        acceptNode(node->while_stmt.cond);

        const auto exitJumpPos = emitter.emitJump(VM_OPCODE_JUMP_IF_FALSE);
        acceptNode(node->while_stmt.body);

        emitter.emitByte(VM_OPCODE_JUMP);
        emitter.emitUint32(loopStartIp);

        emitter.patchJump(exitJumpPos);
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_RETURN_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_YIELD_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_WAIT_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_EXPR_STMT>) {
        acceptNode(node->expr_stmt.expr);
        if (node->expr_stmt.expr->type != ADV_NODE_ASSIGN_EXPR) {
            emitter.emitByte(VM_OPCODE_POP);
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_IMPLICIT_CAST_EXPR>) {
        acceptNode(node->cast_expr.operand);
        emitter.emitCast(node->cast_expr.from_type, node->cast_expr.to_type);
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_EXPLICIT_CAST_EXPR>) {
        acceptNode(node->cast_expr.operand);
        emitter.emitCast(node->cast_expr.from_type, node->cast_expr.to_type);
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_IDENTIFIER_EXPR>) {
        auto& symbolTable = ctx.symbolTable();
        symbolTable.setScope(node->id_expr.scope);
        auto sym = symbolTable.lookup(options, ctx, node->location, node->id_expr.name);
        if (sym->isGlobal) {
            emitter.emitByte(VM_OPCODE_PUSH_GLOBAL_BY_NAME);
            emitter.emitString(sym->name);
        } else {
            emitter.emitByte(VM_OPCODE_PUSH_LOCAL);
            emitter.emitUint16(sym->slotIndex);
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_ASSIGN_EXPR>) {
        acceptNode(node->assign_expr.val);
        auto target = node->assign_expr.target;
        if (target->type == ADV_NODE_IDENTIFIER_EXPR) {
            auto& symbolTable = ctx.symbolTable();
            symbolTable.setScope(target->id_expr.scope);
            auto sym = symbolTable.lookup(options, ctx, node->location, target->id_expr.name);
            if (sym->isGlobal) {
                emitter.emitByte(VM_OPCODE_POP_GLOBAL_BY_NAME);
                emitter.emitString(sym->name);
            } else {
                emitter.emitByte(VM_OPCODE_POP_LOCAL);
                emitter.emitUint16(sym->slotIndex);
            }
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_BINARY_EXPR>) {
        acceptNode(node->binary_expr.left);
        acceptNode(node->binary_expr.right);
        switch (node->binary_expr.op) {
        case '+':
            emitter.emitByte(VM_OPCODE_ADD);
            break;
        case '-':
            emitter.emitByte(VM_OPCODE_SUBTRACT);
            break;
        case '*':
            emitter.emitByte(VM_OPCODE_MULTIPLY);
            break;
        case '/':
            emitter.emitByte(VM_OPCODE_DIVISION);
            break;
        case '%':
            emitter.emitByte(VM_OPCODE_MOD);
            break;
        case '<':
            emitter.emitByte(VM_OPCODE_LESS);
            break;
        case '>':
            emitter.emitByte(VM_OPCODE_GREAT);
            break;
        case Parser::token::LE:
            emitter.emitByte(VM_OPCODE_LESS_EQUAL);
            break;
        case Parser::token::GE:
            emitter.emitByte(VM_OPCODE_GREAT_EQUAL);
            break;
        case Parser::token::EQ:
            emitter.emitByte(VM_OPCODE_EQUAL);
            break;
        case Parser::token::NE:
            emitter.emitByte(VM_OPCODE_NOT_EQUAL);
            break;
        case Parser::token::ASHL:
            emitter.emitByte(VM_OPCODE_ARITHMETIC_SHIFT_LEFT);
            break;
        case Parser::token::ASHR:
            emitter.emitByte(VM_OPCODE_ARITHMETIC_SHIFT_RIGHT);
            break;
        case Parser::token::LSHL:
            emitter.emitByte(VM_OPCODE_LOGIC_SHIFT_LEFT);
            break;
        case Parser::token::LSHR:
            emitter.emitByte(VM_OPCODE_LOGIC_SHIFT_RIGHT);
            break;
        default:
            assert(!"unreachable code");
            break;
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_UNARY_EXPR>) {
        acceptNode(node->unary_expr.operand);

        switch (node->unary_expr.op) {
        case '~':
            emitter.emitByte(VM_OPCODE_BITWISE_NOT);
            break;
        case '-':
            emitter.emitByte(VM_OPCODE_NUMERIC_NEGATION);
            break;
        case Parser::token::NOT:
            emitter.emitByte(VM_OPCODE_LOGICAL_NOT);
            break;
        default:
            break;
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_CALL_EXPR>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_MEMBER_ACCESS>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_BOOL_LITERAL>) {
        emitter.emitByte(node->bool_val ? VM_OPCODE_PUSH_TRUE : VM_OPCODE_PUSH_FALSE);
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_INT_LITERAL>) {
        emitter.emitByte(VM_OPCODE_PUSH_INT);
        emitter.emitInt32(node->int_val);
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_FLOAT_LITERAL>) {
        emitter.emitByte(VM_OPCODE_PUSH_FLOAT);
        emitter.emitFloat(node->float_val);
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_STRING_LITERAL>) {
        emitter.emitByte(VM_OPCODE_PUSH_STRING);
        emitter.emitString(node->str_val);
    }
    void acceptNode(adv_node_t* node) {
        accept<CodeGenVisitor>(node, std::cref(options), std::ref(ctx), std::ref(emitter));
    }
    const Options& options;
    Context& ctx;
    Emitter& emitter;
};

} // namespace

Generator::Generator(const Options& options, Context& ctx) noexcept : _options(options), _ctx(ctx) {}

adv_result_t Generator::gen() {
    if (_ctx.bytecode().empty()) {
        Emitter emitter(_ctx, _data, _code);
        accept<CodeGenVisitor>(_ctx.root(), std::cref(_options), std::ref(_ctx), std::ref(emitter));
        saveBinary();
    }
    return ADV_RESULT_SUCCESS;
}

adv_result_t Generator::text() {
    switch (_options.getOutputFormat()) {
    case ADV_FORMAT_TEXT:
        break;
    case ADV_FORMAT_BINARY:
        return ADV_RESULT_SUCCESS;
    default:
        return ADV_RESULT_ERROR_INVALID_ARG;
    }
    const auto tab = "       "s;
    size_t strLen = 0;
    std::cout << ".format: "sv << "ADVT"sv << std::endl;
    std::cout << ".version: "sv << VM_VERSION_STRING << std::endl;
    size_t literalCount = 0;
    std::map<std::string_view, std::string> names;
    std::map<std::string_view, size_t> strPool;
    size_t labelIndex = 0;
    std::map<size_t, std::string> labels;
    std::vector<std::tuple<
        size_t,
        std::string_view,
        std::variant<std::monostate, adv_sint16_t, adv_sint32_t, adv_float32_t, std::string_view>>>
        codeLines;
    codeLines.emplace_back();
    size_t ip = 0;
    size_t codeSize = _code.size();
    const auto code = _code.data();
    while (ip < codeSize) {
        if (ip > 0) {
            codeLines.emplace_back();
        }
        auto& [index, op, val] = codeLines.back();
        index = ip;
        const auto opcode = (vm_opcode_t)code[ip++];
        auto opcodeName = magic_enum::enum_name(opcode).substr(10);
        op = opcodeName;
        switch (opcode) {
        case VM_OPCODE_PUSH_INT:
            val = Reader::readInt32(code, ip);
            break;
        case VM_OPCODE_PUSH_FLOAT: {
            val = Reader::readFloat(code, ip);
            break;
        }
        case VM_OPCODE_PUSH_STRING: {
            const auto [offset, str] = Reader::readString(code, ip, _data.data());
            val = str;
            ++strPool[str];
            if (!names.contains(str)) {
                names[str] = "lit"s + std::to_string(literalCount++);
            }
            break;
        }
        case VM_OPCODE_PUSH_LOCAL:
        case VM_OPCODE_POP_LOCAL:
            val = Reader::readInt16(code, ip);
            break;
        case VM_OPCODE_POP_GLOBAL_BY_NAME:
        case VM_OPCODE_PUSH_GLOBAL_BY_NAME: {
            const auto [offset, str] = Reader::readString(code, ip, _data.data());
            val = str;
            ++strPool[str];
            if (!names.contains(str)) {
                names[str] = str;
            }
            break;
        }
        case VM_OPCODE_PUSH_GLOBAL:
            assert(!"todo");
            break;
        case VM_OPCODE_POP_GLOBAL:
            assert(!"todo");
            break;
        case VM_OPCODE_CALL_NATIVE:
            assert(!"todo");
            break;
        case VM_OPCODE_CALL:
            assert(!"todo");
            break;
        case VM_OPCODE_JUMP:
        case VM_OPCODE_JUMP_IF_FALSE: {
            const auto address = Reader::readUint32(code, ip);
            if (!labels.contains(address)) {
                labels[address] = "lb"s + std::to_string(labelIndex++);
            }
            const auto& l = labels[address];
            val = std::string_view{l.c_str(), l.length()};
            break;
        }
        case VM_OPCODE_YIELD:
            assert(!"todo");
            break;
        case VM_OPCODE_WAIT:
            assert(!"todo");
            break;
        case VM_OPCODE_HALT:
            break;
        default:
            break;
        }
    }
    auto hasDataSection = false;
    for (const auto& [_, counter] : strPool) {
        if (counter > 1) {
            hasDataSection = true;
            break;
        }
    }
    if (hasDataSection) {
        std::cout << ".data:"sv << std::endl;
        for (const auto& [text, counter] : strPool) {
            if (counter > 1) {
                std::cout << tab << names[text] << ": \""sv << utils::escape(text) << '"' << std::endl;
            }
        }
    }
    if (!codeLines.empty()) {
        std::cout << ".text:"sv << std::endl;
        for (const auto& [index, op, val] : codeLines) {
            std::string label = tab;
            if (auto it = labels.find(index); it != labels.end()) {
                label = fmt::format("{:>5}: ", it->second);
            }
            std::cout << label << op;
            if (val.index() != 0) {
                std::cout << ' ';
                if (std::holds_alternative<std::string_view>(val)) {
                    auto str = std::get<std::string_view>(val);
                    if (strPool.contains(str)) {
                        if (strPool[str] == 1) {
                            std::cout << '"' << utils::escape(str) << '"';
                        } else {
                            std::cout << names[str];
                        }
                    } else {
                        std::cout << str;
                    }
                } else if (std::holds_alternative<adv_sint16_t>(val)) {
                    std::cout << std::get<adv_sint16_t>(val);
                } else if (std::holds_alternative<adv_sint32_t>(val)) {
                    std::cout << std::get<adv_sint32_t>(val);
                } else if (std::holds_alternative<adv_float32_t>(val)) {
                    auto str = fmt::format("{:g}", std::get<adv_float32_t>(val));
                    if (str.find_last_of('.') == std::string::npos) {
                        str += ".0"s;
                    }
                    std::cout << str;
                }
            }
            std::cout << std::endl;
        }
    }
    return ADV_RESULT_SUCCESS;
}

void Generator::saveBinary() {
    constexpr size_t sizeHeader = sizeof(vm_script_header_t);
    constexpr size_t sizeTable = sizeof(vm_script_table_t);
    std::vector<uint8_t> binary(sizeHeader + sizeTable + _data.size() + _code.size());

    auto header = utils::aliasCast<vm_script_header_t*>(binary.data());
    auto table = utils::aliasCast<vm_script_table_t*>(binary.data() + sizeHeader);
    header->magic[0] = 'a';
    header->magic[1] = 'd';
    header->magic[2] = 'v';
    header->magic[3] = 'b';
    header->checksum = 0;
    header->version = ADV_VERSION;
    table->data_offset = sizeHeader + sizeTable;
    table->data_size = static_cast<adv_uint32_t>(_data.size());
    table->text_offset = table->data_offset + table->data_size;
    table->text_size = static_cast<adv_uint32_t>(_code.size());
    memcpy(binary.data() + table->data_offset, _data.data(), table->data_size);
    memcpy(binary.data() + table->text_offset, _code.data(), table->text_size);
    if (std::endian::native != std::endian::little) {
        header->version = utils::swapBytes(header->version);
        table->data_offset = utils::swapBytes(table->data_offset);
        table->data_size = utils::swapBytes(table->data_size);
        table->text_offset = utils::swapBytes(table->text_offset);
        table->text_size = utils::swapBytes(table->text_size);
    }
    auto view = std::views::all(binary);
    header->checksum = utils::adler32(std::move(view));
    if (std::endian::native != std::endian::little) {
        header->checksum = utils::swapBytes(header->checksum);
    }
    _ctx.setBytecode(std::move(binary), utils::Format::Advb);
}

} // namespace adv
