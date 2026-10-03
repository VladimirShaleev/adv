#include "generator.hpp"
#include "context.hpp"
#include "parser.hpp"
#include "visitor.hpp"

using namespace std::literals;

namespace adv {

namespace {

class Emitter {
public:
    Emitter(
        Context& ctx,
        std::unordered_map<vm_string_h, adv_uint32_t>& strings,
        std::vector<uint8_t>& data,
        std::vector<uint8_t>& code) noexcept
        : ctx(ctx), strings(strings), data(data), code(code) {}

    void emitByte(uint8_t byte) { code.push_back(byte); }
    void emitUint16(adv_uint16_t val) { emitUint16(val, code); }
    void emitUint32(adv_uint32_t val) { emitUint32(val, code); }
    void emitInt32(adv_sint32_t val) { emitUint32(static_cast<adv_uint32_t>(val), code); }
    void emitString(vm_string_h str) { emitUint32(pushString(str), code); }
    void emitFloat(adv_float32_t val) {
        static_assert(sizeof(adv_float32_t) == sizeof(adv_uint32_t));
        adv_uint32_t bits;
        std::memcpy(&bits, &val, sizeof(adv_float32_t));
        emitUint32(bits);
    }

    [[nodiscard]] adv_uint32_t emitJump(vm_opcode_t op) {
        emitByte(op);
        const auto patchOffset = offset();
        emitUint32(0, code);
        return patchOffset;
    }

    void patchJump(adv_uint32_t patchOffset) {
        auto targetAddress = utils::swapBytes(offset());
        code[patchOffset + 0] = static_cast<uint8_t>((targetAddress >> 0) & 0xFF);
        code[patchOffset + 1] = static_cast<uint8_t>((targetAddress >> 8) & 0xFF);
        code[patchOffset + 2] = static_cast<uint8_t>((targetAddress >> 16) & 0xFF);
        code[patchOffset + 3] = static_cast<uint8_t>((targetAddress >> 24) & 0xFF);
    }

    void emitCast(vm_value_type_t from, vm_value_type_t to) {
        assert(from != to);
        constexpr vm_opcode_t castOperations[VM_VALUE_TYPE_COUNT_ENUMS][VM_VALUE_TYPE_COUNT_ENUMS] = {
            {VM_OPCODE_NO_OPERATION,
             VM_OPCODE_NO_OPERATION,
             VM_OPCODE_NO_OPERATION,
             VM_OPCODE_NO_OPERATION,
             VM_OPCODE_NO_OPERATION},
            {VM_OPCODE_NO_OPERATION,
             VM_OPCODE_NO_OPERATION,
             VM_OPCODE_BOOL_TO_INT,
             VM_OPCODE_BOOL_TO_FLOAT,
             VM_OPCODE_BOOL_TO_STRING},
            {VM_OPCODE_NO_OPERATION,
             VM_OPCODE_INT_TO_BOOL,
             VM_OPCODE_NO_OPERATION,
             VM_OPCODE_INT_TO_FLOAT,
             VM_OPCODE_INT_TO_STRING},
            {VM_OPCODE_NO_OPERATION,
             VM_OPCODE_FLOAT_TO_BOOL,
             VM_OPCODE_FLOAT_TO_INT,
             VM_OPCODE_NO_OPERATION,
             VM_OPCODE_FLOAT_TO_STRING},
            {VM_OPCODE_NO_OPERATION,
             VM_OPCODE_STRING_TO_BOOL,
             VM_OPCODE_STRING_TO_INT,
             VM_OPCODE_STRING_TO_FLOAT,
             VM_OPCODE_NO_OPERATION},
        };
        const auto castOp = castOperations[from][to];
        assert(castOp != VM_OPCODE_NO_OPERATION);
        emitByte(castOp);
    }

    [[nodiscard]] adv_uint32_t offset() const noexcept { return static_cast<adv_uint32_t>(code.size()); }

    Context& ctx;
    std::unordered_map<vm_string_h, adv_uint32_t>& strings;
    std::vector<uint8_t>& data;
    std::vector<uint8_t>& code;
    adv_uint16_t count{};

private:
    static void emitUint16(adv_uint16_t val, std::vector<uint8_t>& buffer) {
        val = utils::swapBytes(val);
        buffer.push_back(static_cast<uint8_t>((val >> 0) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    }

    static void emitUint32(adv_uint32_t val, std::vector<uint8_t>& buffer) {
        val = utils::swapBytes(val);
        buffer.push_back(static_cast<uint8_t>((val >> 0) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    }

    [[nodiscard]] adv_uint32_t pushString(vm_string_h str) {
        if (auto it = strings.find(str); it != strings.end()) {
            return it->second;
        } else {
            const auto offset = static_cast<adv_uint32_t>(data.size());
            auto view = ctx.getString(str);
            emitUint16(static_cast<adv_uint16_t>(view.length()), data);
            data.insert(data.end(), view.begin(), view.end());
            data.push_back(uint8_t('\0'));
            strings[str] = offset;
            return offset;
        }
    }
};

struct Reader {
    static adv_uint16_t readUint16(const uint8_t* code, size_t& ip) {
        const adv_uint16_t b0 = static_cast<adv_uint16_t>(code[ip + 0]) << 0;
        const adv_uint16_t b1 = static_cast<adv_uint16_t>(code[ip + 1]) << 8;
        const adv_uint16_t val = b0 | b1;
        ip += 2;
        return utils::swapBytes(val);
    }

    static adv_uint32_t readUint32(const uint8_t* code, size_t& ip) {
        const adv_uint32_t b0 = static_cast<adv_uint32_t>(code[ip + 0]) << 0;
        const adv_uint32_t b1 = static_cast<adv_uint32_t>(code[ip + 1]) << 8;
        const adv_uint32_t b2 = static_cast<adv_uint32_t>(code[ip + 2]) << 16;
        const adv_uint32_t b3 = static_cast<adv_uint32_t>(code[ip + 3]) << 24;
        const adv_uint32_t val = b0 | b1 | b2 | b3;
        ip += 4;
        return utils::swapBytes(val);
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
    CodeGenVisitor(
        const Options& options, Context& ctx, Emitter& code, Emitter& exportVar, Emitter& exportFunc) noexcept
        : options(options), ctx(ctx), code(code), exportVar(exportVar), exportFunc(exportFunc) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_PROGRAM>) {
        auto stmt = node->stmts.first_stmt;
        while (stmt) {
            acceptNode(stmt);
            stmt = stmt->next_sibling;
        }
        code.emitByte(VM_OPCODE_HALT);
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_BLOCK>) {
        auto stmt = node->stmts.first_stmt;
        while (stmt) {
            acceptNode(stmt);
            stmt = stmt->next_sibling;
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_VAR_DECL>) {
        const auto sym = ctx.symbolTable().getSymbol(node->var_decl.symbol);
        if (node->var_decl.init_expr) {
            acceptNode(node->var_decl.init_expr);
        } else {
            switch (sym->type) {
            case VM_VALUE_TYPE_BOOL:
                code.emitByte(VM_OPCODE_PUSH_FALSE);
                break;
            case VM_VALUE_TYPE_INT:
                code.emitByte(VM_OPCODE_PUSH_INT);
                code.emitInt32(0);
                break;
            case VM_VALUE_TYPE_FLOAT:
                break;
            case VM_VALUE_TYPE_STRING:
                break;
            default:
                assert(!"unreachable code");
                break;
            }
        }
        if (sym->is_global) {
            code.emitByte(VM_OPCODE_POP_GLOBAL_BY_NAME);
            code.emitString(sym->name);
            exportVar.emitString(sym->name);
            exportVar.emitByte(static_cast<adv_uint8_t>(sym->type));
            exportVar.count++;
        } else {
            code.emitByte(VM_OPCODE_POP_LOCAL);
            code.emitUint16(sym->slot_index);
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_FUNCTION_DECL>) {
        const auto jumpOverBody = code.emitJump(VM_OPCODE_JUMP);
        const auto entryIp = static_cast<uint32_t>(code.offset());

        acceptNode(node->func_decl.body);
        // code.emitByte(VM_OPCODE_PUSH_ZERO);
        // code.emitByte(VM_OPCODE_RETURN);
        code.patchJump(jumpOverBody);

        auto& symbolTable = ctx.symbolTable();
        // symbolTable.setScope(node->func_decl.scope);
        // auto sym = symbolTable.lookup(options, ctx, node->location, node->func_decl.name);
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

        const auto elseJumpPos = code.emitJump(VM_OPCODE_JUMP_IF_FALSE);
        acceptNode(node->if_stmt.then_branch);

        if (node->if_stmt.else_branch) {
            const auto endJumpPos = code.emitJump(VM_OPCODE_JUMP);
            code.patchJump(elseJumpPos);
            acceptNode(node->if_stmt.else_branch);
            code.patchJump(endJumpPos);
        } else {
            code.patchJump(elseJumpPos);
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_WHILE_STMT>) {
        const auto loopStartIp = static_cast<uint32_t>(code.offset());

        acceptNode(node->while_stmt.cond);

        const auto exitJumpPos = code.emitJump(VM_OPCODE_JUMP_IF_FALSE);
        acceptNode(node->while_stmt.body);

        code.emitByte(VM_OPCODE_JUMP);
        code.emitUint32(loopStartIp);

        code.patchJump(exitJumpPos);
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_RETURN_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_YIELD_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_WAIT_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_EXPR_STMT>) {
        acceptNode(node->expr_stmt.expr);
        if (node->expr_stmt.expr->type != ADV_NODE_ASSIGN_EXPR) {
            code.emitByte(VM_OPCODE_POP);
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_IMPLICIT_CAST_EXPR>) {
        acceptNode(node->cast_expr.operand);
        code.emitCast(node->cast_expr.from_type, node->cast_expr.to_type);
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_EXPLICIT_CAST_EXPR>) {
        acceptNode(node->cast_expr.operand);
        code.emitCast(node->cast_expr.from_type, node->cast_expr.to_type);
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_IDENTIFIER_EXPR>) {
        const auto sym = ctx.symbolTable().getSymbol(node->id_expr.symbol);
        if (sym->is_global) {
            code.emitByte(VM_OPCODE_PUSH_GLOBAL_BY_NAME);
            code.emitString(sym->name);
        } else {
            code.emitByte(VM_OPCODE_PUSH_LOCAL);
            code.emitUint16(sym->slot_index);
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_ASSIGN_EXPR>) {
        acceptNode(node->assign_expr.val);
        auto target = node->assign_expr.target;
        if (target->type == ADV_NODE_IDENTIFIER_EXPR) {
            auto sym = ctx.symbolTable().getSymbol(target->id_expr.symbol);
            if (sym->is_global) {
                code.emitByte(VM_OPCODE_POP_GLOBAL_BY_NAME);
                code.emitString(sym->name);
            } else {
                code.emitByte(VM_OPCODE_POP_LOCAL);
                code.emitUint16(sym->slot_index);
            }
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_BINARY_EXPR>) {
        acceptNode(node->binary_expr.left);
        acceptNode(node->binary_expr.right);
        switch (node->binary_expr.op) {
        case '+':
            code.emitByte(VM_OPCODE_ADD);
            break;
        case '-':
            code.emitByte(VM_OPCODE_SUBTRACT);
            break;
        case '*':
            code.emitByte(VM_OPCODE_MULTIPLY);
            break;
        case '/':
            code.emitByte(VM_OPCODE_DIVISION);
            break;
        case '%':
            code.emitByte(VM_OPCODE_MOD);
            break;
        case '<':
            code.emitByte(VM_OPCODE_LESS);
            break;
        case '>':
            code.emitByte(VM_OPCODE_GREAT);
            break;
        case Parser::token::LE:
            code.emitByte(VM_OPCODE_LESS_EQUAL);
            break;
        case Parser::token::GE:
            code.emitByte(VM_OPCODE_GREAT_EQUAL);
            break;
        case Parser::token::EQ:
            code.emitByte(VM_OPCODE_EQUAL);
            break;
        case Parser::token::NE:
            code.emitByte(VM_OPCODE_NOT_EQUAL);
            break;
        case Parser::token::ASHL:
            code.emitByte(VM_OPCODE_ARITHMETIC_SHIFT_LEFT);
            break;
        case Parser::token::ASHR:
            code.emitByte(VM_OPCODE_ARITHMETIC_SHIFT_RIGHT);
            break;
        case Parser::token::LSHL:
            code.emitByte(VM_OPCODE_LOGIC_SHIFT_LEFT);
            break;
        case Parser::token::LSHR:
            code.emitByte(VM_OPCODE_LOGIC_SHIFT_RIGHT);
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
            code.emitByte(VM_OPCODE_BITWISE_NOT);
            break;
        case '-':
            code.emitByte(VM_OPCODE_NUMERIC_NEGATION);
            break;
        case Parser::token::NOT:
            code.emitByte(VM_OPCODE_LOGICAL_NOT);
            break;
        default:
            break;
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_CALL_EXPR>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_MEMBER_ACCESS>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_BOOL_LITERAL>) {
        code.emitByte(node->bool_val ? VM_OPCODE_PUSH_TRUE : VM_OPCODE_PUSH_FALSE);
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_INT_LITERAL>) {
        code.emitByte(VM_OPCODE_PUSH_INT);
        code.emitInt32(node->int_val);
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_FLOAT_LITERAL>) {
        code.emitByte(VM_OPCODE_PUSH_FLOAT);
        code.emitFloat(node->float_val);
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_STRING_LITERAL>) {
        code.emitByte(VM_OPCODE_PUSH_STRING);
        code.emitString(node->str_val);
    }
    void acceptNode(adv_node_t* node) {
        accept<CodeGenVisitor>(
            node, std::cref(options), std::ref(ctx), std::ref(code), std::ref(exportVar), std::ref(exportFunc));
    }
    const Options& options;
    Context& ctx;
    Emitter& code;
    Emitter& exportVar;
    Emitter& exportFunc;
};

} // namespace

Generator::Generator(const Options& options, Context& ctx) noexcept : _options(options), _ctx(ctx) {}

adv_result_t Generator::gen() {
    if (_ctx.bytecode().empty()) {
        std::unordered_map<vm_string_h, adv_uint32_t> strings;
        std::vector<uint8_t> stub;
        std::vector<uint8_t> exportTable;
        std::vector<uint8_t> exportVar;
        std::vector<uint8_t> exportFunc;
        std::vector<uint8_t> data;
        std::vector<uint8_t> code;
        Emitter(_ctx, strings, data, stub).emitString(_ctx.rootModule());
        Emitter emitterCode(_ctx, strings, data, code);
        Emitter emitterExportVar(_ctx, strings, data, exportVar);
        Emitter emitterExportFunc(_ctx, strings, data, exportFunc);
        accept<CodeGenVisitor>(
            _ctx.root(),
            std::cref(_options),
            std::ref(_ctx),
            std::ref(emitterCode),
            std::ref(emitterExportVar),
            std::ref(emitterExportFunc));
        Emitter emitterExport(_ctx, strings, data, exportTable);
        emitterExport.emitUint16(emitterExportVar.count);
        emitterExport.emitUint16(static_cast<adv_uint16_t>(exportVar.size()));
        emitterExport.emitUint16(emitterExportFunc.count);
        emitterExport.emitUint16(static_cast<adv_uint16_t>(exportFunc.size()));
        exportTable.insert(exportTable.end(), exportVar.begin(), exportVar.end());
        exportTable.insert(exportTable.end(), exportFunc.begin(), exportFunc.end());
        saveBinary(exportTable, data, code);
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

    const auto& bytecode = _ctx.bytecode();
    if (!utils::isBinaryCode({(const char*)bytecode.data(), bytecode.size()})) {
        return ADV_RESULT_ERROR_INVALID_ARG;
    }

    const auto table =
        vm_swap_script_table(utils::aliasCast<const vm_script_table_t*>(bytecode.data() + sizeof(vm_script_header_t)));
    const auto exports =
        vm_swap_script_export(utils::aliasCast<const vm_script_export_t*>(bytecode.data() + table.export_offset));
    const auto data = bytecode.data() + table.data_offset;
    const auto text = bytecode.data() + table.text_offset;

    auto readString = [data](size_t strOffset) noexcept {
        const auto strLen = Reader::readUint16(data, strOffset);
        return std::string_view{(const char*)data + strOffset, strLen};
    };

    auto readType = [](size_t index) noexcept {
        switch (index) {
        case VM_VALUE_TYPE_BOOL:
            return "bool"sv;
        case VM_VALUE_TYPE_INT:
            return "int"sv;
        case VM_VALUE_TYPE_FLOAT:
            return "float"sv;
        case VM_VALUE_TYPE_STRING:
            return "string"sv;
        }
        return ""sv;
    };

    constexpr std::string_view tab = "    "sv;
    std::cout << ".header"sv << std::endl;
    std::cout << tab << "format: ADVT"sv << std::endl;
    if (table.data_size > 0) {
        std::cout << tab << "module: "sv << readString(0) << std::endl;
    }
    std::cout << tab << "version: "sv << VM_VERSION_STRING << std::endl;

    std::set<std::string> identifiers;
    identifiers.insert("true");
    identifiers.insert("false");
    auto genIdentifier = [&identifiers](std::string_view str) {
        constexpr size_t maxNameLength = 7;
        std::string base;
        if (str.empty() || !std::isalpha(static_cast<unsigned char>(str.front()))) {
            base = "lit";
        }
        for (char ch : str) {
            if (std::isalnum(ch)) {
                base += static_cast<char>(std::tolower(ch));
            }
            if (base.length() >= maxNameLength) {
                break;
            }
        }
        if (const auto [_, inserted] = identifiers.insert(base); inserted) {
            return base;
        }
        for (size_t suffix = 2;; ++suffix) {
            auto name = base + std::to_string(suffix);
            if (identifiers.insert(name).second) {
                return name;
            }
        }
    };

    if (exports.var_count > 0 || exports.func_count > 0) {
        std::cout << std::endl << ".export"sv << std::endl;
        auto varOffset = table.export_offset + sizeof(vm_script_export_t);
        const auto varBegin = utils::aliasCast<const vm_script_export_var_t*>(bytecode.data() + varOffset);
        for (size_t i = 0; i < exports.var_count; ++i) {
            varOffset += sizeof(vm_script_export_var_t);
            if (varOffset > bytecode.size()) {
                return ADV_RESULT_ERROR_INVALID_ARG;
            }
            const auto var = vm_swap_script_export_var(varBegin + i);
            const auto name = readString(var.name);
            const auto type = readType(var.type);
            if (type.empty()) {
                return ADV_RESULT_ERROR_INVALID_ARG;
            }
            std::cout << tab << "var "sv << name << ": "sv << type << std::endl;
            identifiers.emplace(name.data(), name.length());
        }
        if (exports.func_count > 0) {
            // TODO
        }
    }

    struct Id {
        std::string_view name;
    };
    struct Line {
        size_t index;
        std::string_view op;
        std::variant<std::monostate, adv_sint16_t, adv_sint32_t, adv_float32_t, std::string_view, Id> val;
    };
    std::map<std::string_view, std::pair<size_t, std::string>> strings;
    std::map<adv_uint32_t, std::string> labels;
    std::vector<Line> codeLines;
    codeLines.reserve(table.text_size);
    if (table.text_size > 0) {
        size_t ip = 0;
        size_t codeSize = table.text_size;
        const auto code = text;
        while (ip < codeSize) {
            codeLines.emplace_back();
            auto& [index, op, val] = codeLines.back();
            index = ip;
            const auto opcode = (vm_opcode_t)code[ip++];
            switch (opcode) {
            case VM_OPCODE_PUSH_TRUE:
                op = "push"sv;
                val = Id{"true"};
                break;
            case VM_OPCODE_PUSH_FALSE:
                op = "push"sv;
                val = Id{"false"};
                break;
            case VM_OPCODE_PUSH_INT:
                op = "push"sv;
                val = Reader::readInt32(code, ip);
                break;
            case VM_OPCODE_PUSH_FLOAT: {
                op = "push"sv;
                val = Reader::readFloat(code, ip);
                break;
            }
            case VM_OPCODE_PUSH_STRING: {
                op = "push"sv;
                const auto [_, str] = Reader::readString(code, ip, data);
                val = str;
                if (auto it = strings.find(str); it != strings.end()) {
                    it->second.first++;
                } else {
                    strings[str] = {1, genIdentifier(str)};
                }
                break;
            }
            case VM_OPCODE_POP_GLOBAL_BY_NAME:
            case VM_OPCODE_PUSH_GLOBAL_BY_NAME: {
                op = opcode == VM_OPCODE_PUSH_GLOBAL_BY_NAME ? "push_global"sv : "pop_global"sv;
                const auto [_, str] = Reader::readString(code, ip, data);
                val = Id{str};
                break;
            }
            case VM_OPCODE_PUSH_LOCAL:
            case VM_OPCODE_POP_LOCAL:
                op = opcode == VM_OPCODE_PUSH_LOCAL ? "push_local"sv : "pop_local"sv;
                val = Reader::readInt16(code, ip);
                break;
            case VM_OPCODE_PUSH_GLOBAL:
                assert(!"todo");
                break;
            case VM_OPCODE_POP_GLOBAL:
                assert(!"todo");
                break;
            case VM_OPCODE_BOOL_TO_INT:
                op = "btoi"sv;
                break;
            case VM_OPCODE_BOOL_TO_FLOAT:
                op = "btof"sv;
                break;
            case VM_OPCODE_BOOL_TO_STRING:
                op = "btos"sv;
                break;
            case VM_OPCODE_INT_TO_BOOL:
                op = "itob"sv;
                break;
            case VM_OPCODE_INT_TO_FLOAT:
                op = "itof"sv;
                break;
            case VM_OPCODE_INT_TO_STRING:
                op = "itos"sv;
                break;
            case VM_OPCODE_FLOAT_TO_BOOL:
                op = "ftob"sv;
                break;
            case VM_OPCODE_FLOAT_TO_INT:
                op = "ftoi"sv;
                break;
            case VM_OPCODE_FLOAT_TO_STRING:
                op = "ftos"sv;
                break;
            case VM_OPCODE_STRING_TO_BOOL:
                op = "stob"sv;
                break;
            case VM_OPCODE_STRING_TO_INT:
                op = "stoi"sv;
                break;
            case VM_OPCODE_STRING_TO_FLOAT:
                op = "stof"sv;
                break;
            case VM_OPCODE_BITWISE_NOT:
                assert(!"todo");
                break;
            case VM_OPCODE_LOGICAL_NOT:
                assert(!"todo");
                break;
            case VM_OPCODE_NUMERIC_NEGATION:
                op = "neg"sv;
                break;
            case VM_OPCODE_BITWISE_AND:
                assert(!"todo");
                break;
            case VM_OPCODE_BITWISE_OR:
                assert(!"todo");
                break;
            case VM_OPCODE_BITWISE_XOR:
                assert(!"todo");
                break;
            case VM_OPCODE_LOGIC_SHIFT_LEFT:
                assert(!"todo");
                break;
            case VM_OPCODE_LOGIC_SHIFT_RIGHT:
                assert(!"todo");
                break;
            case VM_OPCODE_ARITHMETIC_SHIFT_LEFT:
                assert(!"todo");
                break;
            case VM_OPCODE_ARITHMETIC_SHIFT_RIGHT:
                assert(!"todo");
                break;
            case VM_OPCODE_ADD:
                op = "add"sv;
                break;
            case VM_OPCODE_SUBTRACT:
                op = "sub"sv;
                break;
            case VM_OPCODE_MULTIPLY:
                op = "mul"sv;
                break;
            case VM_OPCODE_DIVISION:
                op = "div"sv;
                break;
            case VM_OPCODE_MOD:
                op = "mod"sv;
                break;
            case VM_OPCODE_EQUAL:
                op = "eq"sv;
                break;
            case VM_OPCODE_NOT_EQUAL:
                op = "ne"sv;
                break;
            case VM_OPCODE_LESS:
                op = "less"sv;
                break;
            case VM_OPCODE_LESS_EQUAL:
                op = "less_equal"sv;
                break;
            case VM_OPCODE_GREAT:
                op = "great"sv;
                break;
            case VM_OPCODE_GREAT_EQUAL:
                op = "great_equal"sv;
                break;
            case VM_OPCODE_CALL_NATIVE:
                assert(!"todo");
                break;
            case VM_OPCODE_CALL_BY_NAME:
                assert(!"todo");
                break;
            case VM_OPCODE_CALL:
                assert(!"todo");
                break;
            case VM_OPCODE_RETURN:
                assert(!"todo");
                break;
            case VM_OPCODE_JUMP:
            case VM_OPCODE_JUMP_IF_FALSE: {
                op = opcode == VM_OPCODE_JUMP ? "jump"sv : "jfalse"sv;
                const auto address = Reader::readUint32(code, ip);
                if (auto it = labels.find(address); it != labels.end()) {
                    val = Id{it->second};
                } else {
                    const auto label = genIdentifier("label");
                    auto [newIt, _] = labels.try_emplace(address, label);
                    val = Id{newIt->second};
                }
                break;
            }
            case VM_OPCODE_YIELD:
                assert(!"todo");
                break;
            case VM_OPCODE_WAIT:
                assert(!"todo");
                break;
            case VM_OPCODE_HALT:
                op = "halt"sv;
                break;
            default:
                break;
            }
            assert(!op.empty());
        }
    }
    auto hasDataSection = false;
    for (const auto& [_, countAndId] : strings) {
        if (countAndId.first > 1) {
            hasDataSection = true;
            break;
        }
    }
    if (hasDataSection) {
        std::cout << std::endl << ".data"sv << std::endl;
        for (const auto& [text, countAndId] : strings) {
            if (countAndId.first > 1) {
                std::cout << tab << countAndId.second << ": \""sv << utils::escape(text) << '"' << std::endl;
            }
        }
    }
    if (!codeLines.empty()) {
        std::cout << std::endl << ".text"sv << std::endl;
        for (const auto& [index, op, val] : codeLines) {
            std::string label{tab.data(), tab.length()};
            if (auto it = labels.find(index); it != labels.end()) {
                std::cout << it->second << ':' << std::endl;
            }
            std::cout << label << op;
            if (val.index() != 0) {
                std::cout << ' ';
                if (std::holds_alternative<Id>(val)) {
                    auto name = std::get<Id>(val).name;
                    std::cout << name;
                } else if (std::holds_alternative<std::string_view>(val)) {
                    auto str = std::get<std::string_view>(val);
                    if (auto it = strings.find(str); it != strings.end() && it->second.first > 1) {
                        std::cout << it->second.second;
                    } else {
                        std::cout << '"' << utils::escape(str) << '"';
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

void Generator::saveBinary(
    const std::vector<uint8_t>& exportTable, const std::vector<uint8_t>& data, const std::vector<uint8_t>& code) {
    constexpr size_t sizeHeader = sizeof(vm_script_header_t);
    constexpr size_t sizeTable = sizeof(vm_script_table_t);
    std::vector<uint8_t> binary(sizeHeader + sizeTable + exportTable.size() + data.size() + code.size());

    auto header = utils::aliasCast<vm_script_header_t*>(binary.data());
    auto table = utils::aliasCast<vm_script_table_t*>(binary.data() + sizeHeader);
    header->magic[0] = 'a';
    header->magic[1] = 'd';
    header->magic[2] = 'v';
    header->magic[3] = 'b';
    header->version = ADV_VERSION;
    header->checksum = 0;
    table->export_offset = sizeHeader + sizeTable;
    table->export_size = static_cast<adv_uint32_t>(exportTable.size());
    table->data_offset = table->export_offset + table->export_size;
    table->data_size = static_cast<adv_uint32_t>(data.size());
    table->text_offset = table->data_offset + table->data_size;
    table->text_size = static_cast<adv_uint32_t>(code.size());
    *header = vm_swap_script_header(header);
    *table = vm_swap_script_table(table);
    memcpy(binary.data() + table->export_offset, exportTable.data(), table->export_size);
    memcpy(binary.data() + table->data_offset, data.data(), table->data_size);
    memcpy(binary.data() + table->text_offset, code.data(), table->text_size);
    auto view = std::views::all(binary);
    header->checksum = utils::swapBytes(utils::adler32(std::move(view)));
    _ctx.setBytecode(std::move(binary), utils::Format::Advb);
}

} // namespace adv
