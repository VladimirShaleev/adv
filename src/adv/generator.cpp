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
        std::vector<uint8_t>& code,
        std::map<adv_symbol_h, std::pair<adv_uint32_t, adv_node_t*>>* functions = nullptr) noexcept
        : ctx(ctx), strings(strings), data(data), code(code), functions(functions) {}

    void emitByte(uint8_t byte) { code.push_back(byte); }
    void emitUint16(adv_uint16_t val) { emitUint16(val, code); }
    void emitUint32(adv_uint32_t val) { emitUint32(val, code); }
    void emitInt32(adv_sint32_t val) { emitUint32(static_cast<adv_uint32_t>(val), code); }
    void emitString(vm_string_h str) { emitUint32(pushString(str), code); }
    void emitType(adv_symbol_h type) {
        const auto sym = ctx.symbolTable().getSymbol(type);
        assert(sym);
        if (sym->kind == ADV_SYMBOL_KIND_VOID) {
            emitUint32(0);
        } else if (sym->kind == ADV_SYMBOL_KIND_PRIMITIVE) {
            emitUint32(static_cast<adv_uint32_t>(sym->primitive_type));
        } else if (sym->kind == ADV_SYMBOL_KIND_STRUCTURE) {
            const auto dataOffset = pushString(sym->name);
            assert(dataOffset <= (std::numeric_limits<adv_uint32_t>::max() >> 3));
            emitUint32(utils::encodeExportStructureType(dataOffset));
        } else {
            assert(!"unsupported exported type");
        }
    }
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
    std::map<adv_symbol_h, std::pair<adv_uint32_t, adv_node_t*>>* functions;
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

struct CodeGenVisitor {
    CodeGenVisitor(
        Context& ctx,
        std::set<vm_string_h>& modules,
        Emitter& code,
        Emitter& exportVar,
        Emitter& exportStruct,
        Emitter& exportFunc,
        Emitter& exportNative) noexcept
        : ctx(ctx), modules(modules), code(code), exportVar(exportVar), exportStruct(exportStruct),
          exportFunc(exportFunc), exportNative(exportNative) {}
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
        const auto type = ctx.symbolTable().getSymbol(sym->type);
        if (node->var_decl.init_expr) {
            if (type->kind == ADV_SYMBOL_KIND_STRUCTURE) {
                emitCallArgument(node->var_decl.init_expr);
            } else {
                acceptNode(node->var_decl.init_expr);
            }
        } else if (type->kind == ADV_SYMBOL_KIND_PRIMITIVE) {
            switch (type->primitive_type) {
            case VM_VALUE_TYPE_BOOL:
                code.emitByte(VM_OPCODE_PUSH_FALSE);
                break;
            case VM_VALUE_TYPE_INT:
                code.emitByte(VM_OPCODE_PUSH_INT);
                code.emitInt32(0);
                break;
            case VM_VALUE_TYPE_FLOAT:
                code.emitByte(VM_OPCODE_PUSH_FLOAT);
                code.emitFloat(0.0f);
                break;
            case VM_VALUE_TYPE_STRING:
                code.emitByte(VM_OPCODE_PUSH_STRING);
                code.emitString({0});
                break;
            default:
                assert(!"unreachable code");
                break;
            }
        }
        if (sym->is_global) {
            exportVar.emitString(sym->name);
            exportVar.emitType(sym->type);
            exportVar.count++;
        }
        if (type && type->kind == ADV_SYMBOL_KIND_PRIMITIVE) {
            if (sym->is_global) {
                code.emitByte(VM_OPCODE_POP_GLOBAL_BY_NAME);
                code.emitString(sym->name);
            } else {
                code.emitByte(VM_OPCODE_POP_LOCAL);
                code.emitUint16(sym->slot_index);
            }
        } else if (type && type->kind == ADV_SYMBOL_KIND_STRUCTURE) {
            if (node->var_decl.init_expr) {
                emitStructureStore(sym, sym->type, {}, sym->type, node);
            }
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_NATIVE_DECL>) {
        const auto sym = ctx.symbolTable().getSymbol(node->func_decl.symbol);
        if (sym->is_global) {
            exportNative.emitString(sym->name);
            exportNative.emitType(sym->type);
            exportNative.emitByte(static_cast<adv_uint8_t>(sym->linearChildren.size()));
            for (const auto paramHandle : sym->linearChildren) {
                const auto param = ctx.symbolTable().getSymbol(paramHandle);
                exportNative.emitString(param->name);
                exportNative.emitType(param->type);
            }
            ++exportNative.count;
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_FUNCTION_DECL>) {
        const auto jumpOverBody = code.emitJump(VM_OPCODE_JUMP);
        const auto entryIp = static_cast<uint32_t>(code.offset());
        const auto sym = ctx.symbolTable().getSymbol(node->func_decl.symbol);
        const auto type = ctx.symbolTable().getSymbol(sym->type);

        adv_uint8_t lexicalDepth = 0;
        auto parentNode = node->parent;
        while (parentNode) {
            if (parentNode->type == ADV_NODE_FUNCTION_DECL) {
                ++lexicalDepth;
            }
            parentNode = parentNode->parent;
        }

        const auto paramSlots = std::accumulate(
            sym->linearChildren.begin(),
            sym->linearChildren.end(),
            adv_uint16_t(0),
            [table = &ctx.symbolTable()](auto slots, auto symbol) {
                return slots + table->getSymbol(symbol)->slot_count;
            });

        code.emitByte(VM_OPCODE_FUNCTION_PROLOGUE);
        code.emitByte(lexicalDepth);
        code.emitUint16(sym->slot_count);
        code.emitByte(static_cast<adv_uint8_t>(paramSlots));
        code.emitByte(static_cast<adv_uint8_t>(type->slot_count));
        acceptNode(node->func_decl.body);
        if (type->kind == ADV_SYMBOL_KIND_VOID) {
            code.emitByte(VM_OPCODE_RETURN);
        }
        code.patchJump(jumpOverBody);

        if (sym->is_global) {
            exportFunc.emitString(sym->name);
            exportFunc.emitUint32(entryIp);
            exportFunc.emitType(sym->type);
            exportFunc.emitByte(static_cast<adv_uint8_t>(sym->linearChildren.size()));
            for (const auto paramHandle : sym->linearChildren) {
                const auto param = ctx.symbolTable().getSymbol(paramHandle);
                exportFunc.emitString(param->name);
                exportFunc.emitType(param->type);
            }
            ++exportFunc.count;
        } else {
            (*exportFunc.functions)[node->func_decl.symbol] = {entryIp, node};
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_STRUCT_DECL>) {
        const auto sym = ctx.symbolTable().getSymbol(node->struct_decl.symbol);
        if (sym->is_global) {
            exportStruct.emitString(node->struct_decl.name);
            exportStruct.emitUint16(static_cast<adv_uint16_t>(node->struct_decl.field_count));
            auto field = node->struct_decl.first_field;
            while (field) {
                exportStruct.emitString(field->field_decl.name);
                exportStruct.emitType(field->resolved_type);
                field = field->next_sibling;
            }
            ++exportStruct.count;
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_FIELD_DECL>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_PARAM_DECL>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_IMPORT_STMT>) {
        const auto module = ctx.intern(utils::getModuleName(ctx.getString(node->import_stmt.path)));
        if (!modules.contains(module)) {
            modules.insert(module);
            code.emitByte(VM_OPCODE_IMPORT);
            code.emitString(module);
        }
    }
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
    void visit(adv_node_t* node, Tag<ADV_NODE_RETURN_STMT>) {
        if (node->expr_stmt.expr) {
            const auto type = ctx.symbolTable().getSymbol(node->expr_stmt.expr->resolved_type);
            if (type && type->kind == ADV_SYMBOL_KIND_STRUCTURE) {
                emitCallArgument(node->expr_stmt.expr);
            } else {
                acceptNode(node->expr_stmt.expr);
            }
        }
        code.emitByte(VM_OPCODE_RETURN);
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_YIELD_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_WAIT_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_EXPR_STMT>) {
        acceptNode(node->expr_stmt.expr);
        if (node->expr_stmt.expr->type != ADV_NODE_ASSIGN_EXPR &&
            node->expr_stmt.expr->resolved_type != ctx.symbolTable().lookupVoid()) {
            code.emitByte(VM_OPCODE_POP);
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_IMPLICIT_CAST_EXPR>) {
        const auto fromType = ctx.symbolTable().getSymbol(node->cast_expr.from_type);
        const auto toType = ctx.symbolTable().getSymbol(node->cast_expr.to_type);
        if (fromType->kind == ADV_SYMBOL_KIND_PRIMITIVE && toType->kind == ADV_SYMBOL_KIND_PRIMITIVE) {
            acceptNode(node->cast_expr.operand);
            code.emitCast(fromType->primitive_type, toType->primitive_type);
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_EXPLICIT_CAST_EXPR>) {
        const auto fromType = ctx.symbolTable().getSymbol(node->cast_expr.from_type);
        const auto toType = ctx.symbolTable().getSymbol(node->cast_expr.to_type);
        if (fromType->kind == ADV_SYMBOL_KIND_PRIMITIVE && toType->kind == ADV_SYMBOL_KIND_PRIMITIVE) {
            acceptNode(node->cast_expr.operand);
            code.emitCast(fromType->primitive_type, toType->primitive_type);
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_IDENTIFIER_EXPR>) {
        const auto sym = ctx.symbolTable().getSymbol(node->id_expr.symbol);
        if (sym->is_global) {
            code.emitByte(VM_OPCODE_PUSH_GLOBAL_BY_NAME);
            code.emitString(sym->name);
        } else {
            const auto depth = getLexicalDepth(sym->scope, node);
            if (depth == 0) {
                code.emitByte(VM_OPCODE_PUSH_LOCAL);
            } else {
                code.emitByte(VM_OPCODE_PUSH_OUTER_LOCAL);
                code.emitByte(depth);
            }
            code.emitUint16(sym->slot_index);
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_ASSIGN_EXPR>) {
        auto target = node->assign_expr.target;
        const auto targetType = ctx.symbolTable().getSymbol(node->resolved_type);
        if (targetType && targetType->kind == ADV_SYMBOL_KIND_STRUCTURE) {
            emitCallArgument(node->assign_expr.val);
            if (target->type == ADV_NODE_IDENTIFIER_EXPR) {
                const auto sym = ctx.symbolTable().getSymbol(target->id_expr.symbol);
                emitStructureStore(sym, sym->type, {}, sym->type, node);
            } else if (target->type == ADV_NODE_MEMBER_ACCESS) {
                std::vector<adv_node_t*> members;
                auto root = target;
                while (root->type == ADV_NODE_MEMBER_ACCESS) {
                    members.push_back(root);
                    root = root->member_access.target;
                }
                assert(root->type == ADV_NODE_IDENTIFIER_EXPR);
                std::reverse(members.begin(), members.end());
                std::vector<adv_symbol_h> fields;
                fields.reserve(members.size());
                for (const auto member : members) {
                    fields.push_back(member->member_access.symbol);
                }
                const auto sym = ctx.symbolTable().getSymbol(root->id_expr.symbol);
                emitStructureStore(sym, root->resolved_type, fields, node->resolved_type, node);
            }
            return;
        }

        acceptNode(node->assign_expr.val);
        if (target->type == ADV_NODE_IDENTIFIER_EXPR) {
            auto sym = ctx.symbolTable().getSymbol(target->id_expr.symbol);
            if (sym->is_global) {
                code.emitByte(VM_OPCODE_POP_GLOBAL_BY_NAME);
                code.emitString(sym->name);
            } else {
                const auto depth = getLexicalDepth(sym->scope, node);
                if (depth == 0) {
                    code.emitByte(VM_OPCODE_POP_LOCAL);
                } else {
                    code.emitByte(VM_OPCODE_POP_OUTER_LOCAL);
                    code.emitByte(depth);
                }
                code.emitUint16(sym->slot_index);
            }
        } else if (target->type == ADV_NODE_MEMBER_ACCESS) {
            emitMemberAccess(target, true, node);
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
    void visit(adv_node_t* node, Tag<ADV_NODE_CALL_EXPR>) {
        auto arg = node->call_expr.first_arg;
        while (arg) {
            emitCallArgument(arg);
            arg = arg->next_sibling;
        }

        const auto sym = ctx.symbolTable().getSymbol(node->call_expr.symbol);
        if (sym->kind == ADV_SYMBOL_KIND_FUNCTION) {
            if (sym->is_global) {
                code.emitByte(VM_OPCODE_CALL_BY_NAME);
                code.emitString(sym->name);
            } else {
                const auto [entryIp, funcNode] = (*exportFunc.functions)[node->call_expr.symbol];
                code.emitByte(VM_OPCODE_CALL);
                code.emitUint32(entryIp);
            }
        } else {
            code.emitByte(VM_OPCODE_CALL_NATIVE_BY_NAME);
            code.emitString(sym->name);
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_MEMBER_ACCESS>) { emitMemberAccess(node, false, node); }
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
        code.emitByte(VM_OPCODE_PUSH_STRING_FROM_DATA);
        code.emitString(node->str_val);
    }
    void acceptNode(adv_node_t* node) {
        accept<CodeGenVisitor>(
            node,
            std::ref(ctx),
            std::ref(modules),
            std::ref(code),
            std::ref(exportVar),
            std::ref(exportStruct),
            std::ref(exportFunc),
            std::ref(exportNative));
    }
    void emitCallArgument(adv_node_t* node) {
        const auto type = ctx.symbolTable().getSymbol(node->resolved_type);
        assert(type);
        if (type->kind != ADV_SYMBOL_KIND_STRUCTURE) {
            acceptNode(node);
            return;
        }
        if (node->type == ADV_NODE_CALL_EXPR) {
            acceptNode(node);
            return;
        }

        std::vector<adv_node_t*> members;
        auto root = node;
        while (root->type == ADV_NODE_MEMBER_ACCESS) {
            members.push_back(root);
            root = root->member_access.target;
        }
        if (root->type == ADV_NODE_CALL_EXPR) {
            acceptNode(node);
            return;
        }
        if (root->type != ADV_NODE_IDENTIFIER_EXPR) {
            assert(!"unsupported structure argument expression");
            return;
        }
        std::reverse(members.begin(), members.end());

        std::vector<adv_symbol_h> fields;
        fields.reserve(members.size());
        for (const auto member : members) {
            fields.push_back(member->member_access.symbol);
        }
        const auto rootSymbol = ctx.symbolTable().getSymbol(root->id_expr.symbol);
        emitStructureArgument(rootSymbol, root->resolved_type, type, fields, node);
    }
    void emitStructureArgument(
        Symbol* rootSymbol,
        adv_symbol_h rootType,
        Symbol* structure,
        std::vector<adv_symbol_h>& fields,
        adv_node_t* referenceNode) {
        for (const auto fieldHandle : structure->linearChildren) {
            const auto field = ctx.symbolTable().getSymbol(fieldHandle);
            assert(field);
            const auto fieldType = ctx.symbolTable().getSymbol(field->type);
            assert(fieldType);
            fields.push_back(fieldHandle);
            if (fieldType->kind == ADV_SYMBOL_KIND_STRUCTURE) {
                emitStructureArgument(rootSymbol, rootType, fieldType, fields, referenceNode);
            } else {
                emitFieldPath(rootSymbol, rootType, fields, false, referenceNode);
            }
            fields.pop_back();
        }
    }
    void emitMemberAccess(adv_node_t* node, bool isStore, adv_node_t* referenceNode) {
        std::vector<adv_node_t*> members;
        auto root = node;
        while (root->type == ADV_NODE_MEMBER_ACCESS) {
            members.push_back(root);
            root = root->member_access.target;
        }
        if (root->type == ADV_NODE_CALL_EXPR) {
            std::reverse(members.begin(), members.end());
            emitCallMemberAccess(node, root, members);
            return;
        }
        if (root->type != ADV_NODE_IDENTIFIER_EXPR || members.empty()) {
            assert(!"unsupported structure field access target");
            return;
        }
        std::reverse(members.begin(), members.end());

        std::vector<adv_symbol_h> fields;
        fields.reserve(members.size());
        for (const auto member : members) {
            fields.push_back(member->member_access.symbol);
        }
        const auto rootSymbol = ctx.symbolTable().getSymbol(root->id_expr.symbol);
        emitFieldPath(rootSymbol, root->resolved_type, fields, isStore, referenceNode);
    }
    void emitCallMemberAccess(adv_node_t* node, adv_node_t* call, const std::vector<adv_node_t*>& members) {
        assert(!members.empty());
        const auto callType = ctx.symbolTable().getSymbol(call->resolved_type);
        assert(callType && callType->kind == ADV_SYMBOL_KIND_STRUCTURE);
        const auto frameSlotCount = getFrameSlotCount(call);
        assert(frameSlotCount >= callType->slot_count);
        const auto tempSlotIndex = static_cast<adv_uint16_t>(frameSlotCount - callType->slot_count);
        acceptNode(call);
        for (auto slot = callType->slot_count; slot > 0; --slot) {
            code.emitByte(VM_OPCODE_POP_LOCAL);
            code.emitUint16(static_cast<adv_uint16_t>(tempSlotIndex + slot - 1));
        }

        adv_uint32_t fieldOffset = 0;
        for (const auto member : members) {
            const auto field = ctx.symbolTable().getSymbol(member->member_access.symbol);
            assert(field);
            fieldOffset += field->slot_index;
        }
        const auto resultType = ctx.symbolTable().getSymbol(node->resolved_type);
        assert(resultType);
        const auto resultSlotCount =
            resultType->kind == ADV_SYMBOL_KIND_STRUCTURE ? resultType->slot_count : adv_uint16_t(1);
        for (adv_uint32_t slot = 0; slot < resultSlotCount; ++slot) {
            code.emitByte(VM_OPCODE_PUSH_LOCAL);
            code.emitUint16(static_cast<adv_uint16_t>(tempSlotIndex + fieldOffset + slot));
        }
    }
    adv_uint16_t getFrameSlotCount(adv_node_t* node) {
        auto parent = node->parent;
        while (parent) {
            if (parent->type == ADV_NODE_FUNCTION_DECL) {
                const auto function = ctx.symbolTable().getSymbol(parent->func_decl.symbol);
                assert(function);
                return function->slot_count;
            }
            parent = parent->parent;
        }
        return ctx.rootFrameSlots();
    }
    void emitFieldPath(
        Symbol* rootSymbol,
        adv_symbol_h rootType,
        const std::vector<adv_symbol_h>& fields,
        bool isStore,
        adv_node_t* referenceNode) {
        assert(rootSymbol && !fields.empty());
        if (rootSymbol->is_global) {
            std::string name{ctx.getString(rootSymbol->name)};
            auto structureHandle = rootType;
            for (const auto fieldHandle : fields) {
                const auto structure = ctx.symbolTable().getSymbol(structureHandle);
                assert(structure && structure->kind == ADV_SYMBOL_KIND_STRUCTURE);
                const auto fieldIt =
                    std::find(structure->linearChildren.begin(), structure->linearChildren.end(), fieldHandle);
                assert(fieldIt != structure->linearChildren.end());
                name += '.';
                name += std::to_string(std::distance(structure->linearChildren.begin(), fieldIt));
                structureHandle = ctx.symbolTable().getSymbol(fieldHandle)->type;
            }
            code.emitByte(isStore ? VM_OPCODE_POP_GLOBAL_BY_NAME : VM_OPCODE_PUSH_GLOBAL_BY_NAME);
            code.emitString(ctx.intern(name));
            return;
        }

        adv_uint32_t slot = rootSymbol->slot_index;
        for (const auto fieldHandle : fields) {
            const auto field = ctx.symbolTable().getSymbol(fieldHandle);
            assert(field);
            slot += field->slot_index;
        }

        const auto depth = getLexicalDepth(rootSymbol->scope, referenceNode);
        if (depth == 0) {
            code.emitByte(isStore ? VM_OPCODE_POP_LOCAL : VM_OPCODE_PUSH_LOCAL);
        } else {
            code.emitByte(isStore ? VM_OPCODE_POP_OUTER_LOCAL : VM_OPCODE_PUSH_OUTER_LOCAL);
            code.emitByte(depth);
        }
        code.emitUint16(static_cast<adv_uint16_t>(slot));
    }
    void emitStructureStore(
        Symbol* rootSymbol,
        adv_symbol_h rootType,
        std::vector<adv_symbol_h> fields,
        adv_symbol_h structureType,
        adv_node_t* referenceNode) {
        std::vector<std::vector<adv_symbol_h>> leafPaths;
        collectStructureLeafPaths(ctx.symbolTable().getSymbol(structureType), fields, leafPaths);
        for (auto path = leafPaths.rbegin(); path != leafPaths.rend(); ++path) {
            emitFieldPath(rootSymbol, rootType, *path, true, referenceNode);
        }
    }
    void collectStructureLeafPaths(
        Symbol* structure, std::vector<adv_symbol_h>& fields, std::vector<std::vector<adv_symbol_h>>& leafPaths) {
        assert(structure && structure->kind == ADV_SYMBOL_KIND_STRUCTURE);
        for (const auto fieldHandle : structure->linearChildren) {
            const auto field = ctx.symbolTable().getSymbol(fieldHandle);
            assert(field);
            const auto fieldType = ctx.symbolTable().getSymbol(field->type);
            assert(fieldType);
            fields.push_back(fieldHandle);
            if (fieldType->kind == ADV_SYMBOL_KIND_STRUCTURE) {
                collectStructureLeafPaths(fieldType, fields, leafPaths);
            } else {
                leafPaths.push_back(fields);
            }
            fields.pop_back();
        }
    }
    [[nodiscard]] adv_uint8_t getLexicalDepth(adv_scope_h scope, adv_node_t* node) {
        auto getParentScope = [this, &node]() {
            if (node) {
                node = node->parent;
            }
            while (node && node->type != ADV_NODE_FUNCTION_DECL) {
                node = node->parent;
            }
            return node ? ctx.symbolTable().getSymbol(node->func_decl.symbol)->scope : adv_scope_h{};
        };
        vm_uint8_t depth = 0;
        scope = ctx.symbolTable().getScope(scope)->parent;
        while (node && scope != getParentScope()) {
            ++depth;
        }
        return depth;
    }
    Context& ctx;
    std::set<vm_string_h>& modules;
    Emitter& code;
    Emitter& exportVar;
    Emitter& exportStruct;
    Emitter& exportFunc;
    Emitter& exportNative;
};

} // namespace

Generator::Generator(const Options& options, Context& ctx) noexcept : _options(options), _ctx(ctx) {}

adv_result_t Generator::gen() {
    if (_ctx.bytecode().empty()) {
        std::set<vm_string_h> modules;
        std::unordered_map<vm_string_h, adv_uint32_t> strings;
        std::map<adv_symbol_h, std::pair<adv_uint32_t, adv_node_t*>> functions;
        std::vector<uint8_t> exportTable;
        std::vector<uint8_t> exportVar;
        std::vector<uint8_t> exportStruct;
        std::vector<uint8_t> exportFunc;
        std::vector<uint8_t> exportNative;
        std::vector<uint8_t> data;
        std::vector<uint8_t> code;
        Emitter emitterCode(_ctx, strings, data, code);
        Emitter emitterExportVar(_ctx, strings, data, exportVar);
        Emitter emitterExportStruct(_ctx, strings, data, exportStruct);
        Emitter emitterExportFunc(_ctx, strings, data, exportFunc, &functions);
        Emitter emitterExportNative(_ctx, strings, data, exportNative);
        accept<CodeGenVisitor>(
            _ctx.root(),
            std::ref(_ctx),
            std::ref(modules),
            std::ref(emitterCode),
            std::ref(emitterExportVar),
            std::ref(emitterExportStruct),
            std::ref(emitterExportFunc),
            std::ref(emitterExportNative));
        Emitter emitterExport(_ctx, strings, data, exportTable);
        emitterExport.emitString(_ctx.rootModule());
        emitterExport.emitUint16(_ctx.rootFrameSlots());
        emitterExport.emitUint16(emitterExportStruct.count);
        emitterExport.emitUint16(static_cast<adv_uint16_t>(exportStruct.size()));
        emitterExport.emitUint16(emitterExportVar.count);
        emitterExport.emitUint16(static_cast<adv_uint16_t>(exportVar.size()));
        emitterExport.emitUint16(emitterExportFunc.count);
        emitterExport.emitUint16(static_cast<adv_uint16_t>(exportFunc.size()));
        emitterExport.emitUint16(emitterExportNative.count);
        emitterExport.emitUint16(static_cast<adv_uint16_t>(exportNative.size()));
        exportTable.insert(exportTable.end(), exportStruct.begin(), exportStruct.end());
        exportTable.insert(exportTable.end(), exportVar.begin(), exportVar.end());
        exportTable.insert(exportTable.end(), exportFunc.begin(), exportFunc.end());
        exportTable.insert(exportTable.end(), exportNative.begin(), exportNative.end());
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

    const auto data = utils::read<utils::SectionData>(bytecode).data;
    const auto text = utils::read<utils::SectionText>(bytecode).text;

    auto readType = [&](vm_uint32_t type) noexcept {
        const auto baseType = utils::exportTypeBase(type);
        if (baseType > VM_VALUE_TYPE_STRING) {
            return utils::readString(utils::exportTypeDataOffset(type), data);
        }
        switch (baseType) {
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
    std::ostringstream ss;
    ss << ".header"sv << std::endl;
    ss << tab << "format: ADVT"sv << std::endl;
    ss << tab << "version: "sv << VM_VERSION_STRING << std::endl;

    std::set<std::string> identifiers;
    identifiers.insert("true");
    identifiers.insert("false");
    auto genIdentifier = [&identifiers](std::string_view str, size_t maxNameLength = 7) -> std::string_view {
        std::string base;
        if (str.empty() || !std::isalpha(static_cast<unsigned char>(str.front()))) {
            if (str.front() != '@') {
                base = "lit";
            }
        }
        for (char ch : str) {
            if (std::isalnum(ch) || ch == '@') {
                base += static_cast<char>(std::tolower(ch));
            }
            if (base.length() >= maxNameLength) {
                break;
            }
        }
        if (const auto [it, inserted] = identifiers.insert(base); inserted) {
            return *it;
        }
        for (size_t suffix = 2;; ++suffix) {
            auto name = base + std::to_string(suffix);
            if (auto [it, inserted] = identifiers.insert(name); inserted) {
                return *it;
            }
        }
    };
    auto genLabelIdentifier = [&]() { return genIdentifier("label"); };
    auto getFuncIdentifier = [&]() { return genIdentifier("@function", 15); };

    struct Label {
        std::string_view name;
        bool isGlobal;
        bool isFunction;
    };
    std::unordered_map<std::string_view, adv_uint32_t> functions;
    const auto exports = utils::read<vm_script_export_t>(bytecode);
    const auto moduleName = utils::readString(exports.module_name, data);
    std::map<adv_uint32_t, Label> labels;
    ss << std::endl << ".export"sv << std::endl;
    ss << tab << "module "sv << moduleName << " "sv << exports.root_frame_slots << std::endl;
    for (size_t i = 0; i < exports.struct_count; ++i) {
        const auto structure = utils::read<vm_script_export_struct_t>(bytecode, i);
        const auto name = utils::readString(structure.name, data);
        const bool inlineFields = structure.field_count <= 4;
        ss << tab << "struct "sv << name << " {"sv;
        if (inlineFields && structure.field_count > 0) {
            ss << ' ';
        } else if (!inlineFields) {
            ss << std::endl;
        }
        for (size_t f = 0; f < structure.field_count; ++f) {
            const auto field = utils::read<vm_script_export_field_t>(bytecode, i, f);
            const auto fieldName = utils::readString(field.name, data);
            const auto type = readType(field.type);
            if (type.empty()) {
                return ADV_RESULT_ERROR_INVALID_ARG;
            }
            if (inlineFields) {
                if (f > 0) {
                    ss << ' ';
                }
                ss << fieldName << ": "sv << type << ';';
            } else {
                ss << tab << tab << fieldName << ": "sv << type << ';' << std::endl;
            }
        }
        if (inlineFields) {
            if (structure.field_count > 0) {
                ss << ' ';
            }
            ss << '}' << std::endl;
        } else {
            ss << tab << '}' << std::endl;
        }
        if (const auto [_, inserted] = identifiers.emplace(name.data(), name.length()); !inserted) {
            return ADV_RESULT_ERROR_INVALID_ARG;
        }
    }
    for (size_t i = 0; i < exports.var_count; ++i) {
        const auto var = utils::read<vm_script_export_var_t>(bytecode, i);
        const auto name = utils::readString(var.name, data);
        const auto type = readType(var.type);
        if (type.empty()) {
            return ADV_RESULT_ERROR_INVALID_ARG;
        }
        ss << tab << "var "sv << name << ": "sv << type << std::endl;
        if (const auto [_, inserted] = identifiers.emplace(name.data(), name.length()); !inserted) {
            return ADV_RESULT_ERROR_INVALID_ARG;
        }
    }
    for (size_t i = 0; i < exports.native_count; ++i) {
        const auto native = utils::read<vm_script_export_native_t>(bytecode, i);
        const auto name = utils::readString(native.name, data);
        const auto ret = readType(native.return_type);
        ss << tab << "native "sv << name << '(';
        for (size_t p = 0; p < native.param_count; ++p) {
            const auto param = utils::read<utils::NativeParam>(bytecode, i, p).param;
            const auto name = utils::readString(param.name, data);
            const auto type = readType(param.type);
            if (type.empty()) {
                return ADV_RESULT_ERROR_INVALID_ARG;
            }
            if (p > 0) {
                ss << ", "sv;
            }
            ss << name << ": "sv << type;
        }
        ss << ')';
        if (!ret.empty()) {
            ss << ": "sv << ret;
        }
        ss << std::endl;
        if (const auto [_, inserted] = identifiers.emplace(name.data(), name.length()); !inserted) {
            return ADV_RESULT_ERROR_INVALID_ARG;
        }
    }
    for (size_t i = 0; i < exports.func_count; ++i) {
        const auto fun = utils::read<vm_script_export_func_t>(bytecode, i);
        const auto name = utils::readString(fun.name, data);
        const auto ret = readType(fun.return_type);
        ss << tab << "function "sv << name << '(';
        for (size_t p = 0; p < fun.param_count; ++p) {
            const auto var = utils::read<vm_script_export_param_t>(bytecode, i, p);
            const auto name = utils::readString(var.name, data);
            const auto type = readType(var.type);
            if (type.empty()) {
                return ADV_RESULT_ERROR_INVALID_ARG;
            }
            if (p > 0) {
                ss << ", "sv;
            }
            ss << name << ": "sv << type;
        }
        ss << ')';
        if (!ret.empty()) {
            ss << ": "sv << ret;
        }
        ss << std::endl;
        if (const auto [_, inserted] = identifiers.emplace(name.data(), name.length()); !inserted) {
            return ADV_RESULT_ERROR_INVALID_ARG;
        }
        functions[name] = fun.text_offset;
        labels.try_emplace(fun.text_offset, name, true, true);
    }

    struct Id {
        std::string_view name;
    };
    struct Function {
        adv_uint8_t lexicalDepth;
        adv_uint16_t frameSlotCount;
        adv_uint8_t paramCount;
        adv_uint8_t returnCount;
    };
    struct Outer {
        adv_uint8_t frameIndex;
        adv_uint16_t slot;
    };
    struct Line {
        size_t index;
        std::string_view op;
        std::variant<std::monostate, adv_uint16_t, adv_sint32_t, adv_float32_t, std::string_view, Id, Function, Outer>
            val;
    };
    std::map<std::string_view, std::pair<size_t, std::string_view>> strings;
    std::vector<Line> codeLines;
    codeLines.reserve(text.size());
    if (!text.empty()) {
        vm_uint32_t ip = 0;
        while (ip < text.size()) {
            codeLines.emplace_back();
            auto& [index, op, val] = codeLines.back();
            index = static_cast<size_t>(ip);
            const auto opcode = (vm_opcode_t)text.data()[ip++];
            switch (opcode) {
            case VM_OPCODE_IMPORT:
                op = "import"sv;
                val = Id{utils::readString(text, ip, data)};
                break;
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
                val = utils::readInt32(text, ip);
                break;
            case VM_OPCODE_PUSH_FLOAT: {
                op = "push"sv;
                val = utils::readFloat(text, ip);
                break;
            }
            case VM_OPCODE_PUSH_STRING_FROM_DATA: {
                op = "push"sv;
                const auto str = utils::readString(text, ip, data);
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
                val = Id{utils::readString(text, ip, data)};
                break;
            }
            case VM_OPCODE_PUSH_LOCAL:
            case VM_OPCODE_POP_LOCAL:
                op = opcode == VM_OPCODE_PUSH_LOCAL ? "push_local"sv : "pop_local"sv;
                val = utils::readUint16(text, ip);
                break;
            case VM_OPCODE_PUSH_OUTER_LOCAL:
            case VM_OPCODE_POP_OUTER_LOCAL:
                op = opcode == VM_OPCODE_PUSH_OUTER_LOCAL ? "push_outer"sv : "pop_outer"sv;
                val = Outer{text.data()[ip++], utils::readUint16(text, ip)};
                break;
            case VM_OPCODE_PUSH_GLOBAL:
            case VM_OPCODE_POP_GLOBAL:
                return ADV_RESULT_ERROR_INVALID_ARG;
            case VM_OPCODE_POP:
                op = "pop"sv;
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
                op = "bits_not"sv;
                break;
            case VM_OPCODE_LOGICAL_NOT:
                op = "logic_not"sv;
                break;
            case VM_OPCODE_NUMERIC_NEGATION:
                op = "neg"sv;
                break;
            case VM_OPCODE_BITWISE_AND:
                op = "and"sv;
                break;
            case VM_OPCODE_BITWISE_OR:
                op = "or"sv;
                break;
            case VM_OPCODE_BITWISE_XOR:
                op = "xor"sv;
                break;
            case VM_OPCODE_LOGIC_SHIFT_LEFT:
                op = "lsl"sv;
                break;
            case VM_OPCODE_LOGIC_SHIFT_RIGHT:
                op = "lsr"sv;
                break;
            case VM_OPCODE_ARITHMETIC_SHIFT_LEFT:
                op = "asl"sv;
                break;
            case VM_OPCODE_ARITHMETIC_SHIFT_RIGHT:
                op = "asr"sv;
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
            case VM_OPCODE_FUNCTION_PROLOGUE: {
                const auto address = static_cast<adv_uint32_t>(index);
                const auto lexicalDepth = text.data()[ip++];
                const auto frameSlotCount = utils::readUint16(text, ip);
                const auto paramCount = text.data()[ip++];
                const auto returnCount = text.data()[ip++];
                if (auto it = labels.find(address); it == labels.end()) {
                    const auto label = getFuncIdentifier();
                    labels.try_emplace(address, label, false, true);
                }
                val = Function{lexicalDepth, frameSlotCount, paramCount, returnCount};
                op = "@"sv;
                break;
            }
            case VM_OPCODE_CALL_NATIVE:
                return ADV_RESULT_ERROR_INVALID_ARG;
            case VM_OPCODE_CALL_NATIVE_BY_NAME:
            case VM_OPCODE_CALL_BY_NAME: {
                op = opcode == VM_OPCODE_CALL_NATIVE_BY_NAME ? "call_native"sv : "call"sv;
                const auto name = utils::readString(text, ip, data);
                val = Id{name};
                break;
            }
            case VM_OPCODE_CALL: {
                op = "call"sv;
                const auto address = utils::readUint32(text, ip);
                if (auto it = labels.find(address); it != labels.end()) {
                    val = Id{it->second.name};
                } else {
                    const auto label = getFuncIdentifier();
                    auto [newIt, _] = labels.try_emplace(address, Label{label, false, true});
                    val = Id{newIt->second.name};
                }
                break;
            }
            case VM_OPCODE_RETURN:
                op = "ret"sv;
                break;
            case VM_OPCODE_JUMP:
            case VM_OPCODE_JUMP_IF_FALSE: {
                op = opcode == VM_OPCODE_JUMP ? "jump"sv : "jfalse"sv;
                const auto address = utils::readUint32(text, ip);
                if (auto it = labels.find(address); it != labels.end()) {
                    val = Id{it->second.name};
                } else {
                    const auto label = genLabelIdentifier();
                    auto [newIt, _] = labels.try_emplace(address, Label{label, false, false});
                    val = Id{newIt->second.name};
                }
                break;
            }
            case VM_OPCODE_YIELD:
                op = "yield"sv;
                break;
            case VM_OPCODE_WAIT:
                op = "wait"sv;
                break;
            case VM_OPCODE_HALT:
                op = "halt"sv;
                break;
            case VM_OPCODE_NO_OPERATION:
                op = "nop"sv;
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
        ss << std::endl << ".data"sv << std::endl;
        for (const auto& [text, countAndId] : strings) {
            if (countAndId.first > 1) {
                ss << tab << countAndId.second << ": \""sv << utils::escape(text) << '"' << std::endl;
            }
        }
    }
    if (!codeLines.empty()) {
        ss << std::endl << ".text"sv << std::endl;
        std::string_view prevOp = "";
        for (const auto& [index, op, val] : codeLines) {
            std::string label{tab.data(), tab.length()};
            if (auto it = labels.find(index); it != labels.end()) {
                if (it->second.isGlobal || prevOp == "ret"sv) {
                    ss << std::endl;
                }
                if (it->second.isFunction) {
                    if (op != "@"sv) {
                        return ADV_RESULT_ERROR_INVALID_ARG;
                    }
                    ss << ".function "sv;
                }
                ss << it->second.name;
                if (it->second.isFunction) {
                    const auto& func = std::get<Function>(val);
                    const auto hasOperands =
                        func.lexicalDepth > 0 || func.frameSlotCount > 0 || func.paramCount > 0 || func.returnCount > 0;
                    if (hasOperands) {
                        auto hasPrev = false;
                        std::array<std::pair<std::string_view, adv_uint32_t>, 4> counts{
                            std::make_pair("depth: "sv, static_cast<adv_uint32_t>(func.lexicalDepth)),
                            std::make_pair("slots: "sv, static_cast<adv_uint32_t>(func.frameSlotCount)),
                            std::make_pair("params: "sv, static_cast<adv_uint32_t>(func.paramCount)),
                            std::make_pair("rets: "sv, static_cast<adv_uint32_t>(func.returnCount))};
                        ss << " ("sv;
                        for (const auto& [label, count] : counts) {
                            if (count > 0) {
                                if (hasPrev) {
                                    ss << ", "sv;
                                }
                                ss << label << count;
                                hasPrev = true;
                            }
                        }
                        ss << ')';
                    }
                }
                ss << ':' << std::endl;
                if (it->second.isFunction) {
                    continue;
                }
            }
            ss << label << op;
            if (val.index() != 0) {
                ss << ' ';
                if (std::holds_alternative<Id>(val)) {
                    auto name = std::get<Id>(val).name;
                    ss << name;
                } else if (std::holds_alternative<std::string_view>(val)) {
                    auto str = std::get<std::string_view>(val);
                    if (auto it = strings.find(str); it != strings.end() && it->second.first > 1) {
                        ss << it->second.second;
                    } else {
                        ss << '"' << utils::escape(str) << '"';
                    }
                } else if (std::holds_alternative<Outer>(val)) {
                    const auto& outer = std::get<Outer>(val);
                    ss << (int)outer.frameIndex << ' ' << outer.slot;
                } else if (std::holds_alternative<adv_uint16_t>(val)) {
                    ss << std::get<adv_uint16_t>(val);
                } else if (std::holds_alternative<adv_sint32_t>(val)) {
                    ss << std::get<adv_sint32_t>(val);
                } else if (std::holds_alternative<adv_float32_t>(val)) {
                    auto str = fmt::format("{:g}", std::get<adv_float32_t>(val));
                    if (str.find_last_of('.') == std::string::npos) {
                        str += ".0"s;
                    }
                    ss << str;
                }
            }
            ss << std::endl;
            prevOp = op;
        }
    }
    const auto textAsm = ss.str();
    const auto begin = (const uint8_t*)textAsm.data();
    const auto end = begin + textAsm.length();
    std::vector<adv_uint8_t> result(begin, end);
    _ctx.setBytecode(moduleName, std::move(result), utils::Format::Advt);
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
    *header = utils::swapBytes(*header);
    *table = utils::swapBytes(*table);
    memcpy(binary.data() + table->export_offset, exportTable.data(), table->export_size);
    memcpy(binary.data() + table->data_offset, data.data(), table->data_size);
    memcpy(binary.data() + table->text_offset, code.data(), table->text_size);
    auto view = std::views::all(binary);
    header->checksum = utils::swapBytes(utils::adler32(std::move(view)));
    const auto moduleNameView = _ctx.getString(_ctx.rootModule());
    const std::string moduleName{moduleNameView.data(), moduleNameView.length()};
    _ctx.setBytecode(moduleName, std::move(binary), utils::Format::Advb);
}

} // namespace adv
