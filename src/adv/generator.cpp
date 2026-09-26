#include "generator.hpp"
#include "context.hpp"
#include "visitor.hpp"

#include <iostream> // TODO: remove later

using namespace std::literals;

namespace adv {

namespace {

struct Emitter {
    // void emitByte(uint8_t byte) { buffer.push_back(byte); }
    // void emitInt32(adv_sint32_t val) {
    //     val = std::endian::native == std::endian::little ? val : swap_bytes(val);
    //     buffer.push_back(static_cast<uint8_t>((val >> 0) & 0xFF));
    //     buffer.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    //     buffer.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    //     buffer.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    // }
    // void emitFloat(adv_float32_t val) {
    //     static_assert(sizeof(float) == sizeof(adv_sint32_t));
    //     adv_sint32_t bits;
    //     std::memcpy(&bits, &val, sizeof(float));
    //     emitInt32(bits);
    // }
    // void emitString(std::string_view str) {
    //     for (char c : str) {
    //         emitByte(static_cast<uint8_t>(c));
    //     }
    //     emitByte(0); // null-terminated
    // }
};

    bool hasPrevOpcode{}; // todo
struct EmitterText {
    explicit EmitterText(Context& ctx) noexcept : ctx(ctx) {}

    void emitOpcode(vm_opcode_t op) {
        if (hasPrevOpcode) {
            std::cout << std::endl;
        }
        hasPrevOpcode = true;
        auto code = magic_enum::enum_name(op).substr(10);
        std::cout << "OP_"sv << code;
    }

    void emitByte(uint8_t byte) { std::cout << ' ' << byte; }

    void emitString(vm_string_h str) {
        std::cout << ' ' << str.handle << " ("sv << ctx.getString(str) << ')';
    }

    Context& ctx;
};

template <typename E>
struct CodeGenVisitor {
    CodeGenVisitor(const Options& options, Context& ctx, std::vector<uint8_t>& buffer) noexcept
        : options(options), ctx(ctx), buffer(buffer), emitter(ctx) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_PROGRAM>) {
        auto stmt = node->stmts.first_stmt;
        while (stmt) {
            acceptNode(stmt);
            stmt = stmt->next_sibling;
        }
        emitter.emitOpcode(VM_OPCODE_HALT);
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
            emitter.emitOpcode(VM_OPCODE_PUSH_ZERO);
        }
        auto& symbolTable = ctx.symbolTable();
        symbolTable.setScope(node->var_decl.scope);
        auto sym = symbolTable.lookup(options, ctx, node->location, node->var_decl.name);
        if (sym->isGlobal) {
            emitter.emitOpcode(VM_OPCODE_SET_GLOBAL_BY_NAME);
            emitter.emitString(sym->name);
        } else {
            emitter.emitOpcode(VM_OPCODE_PUSH_LOCAL);
            emitter.emitByte(static_cast<uint8_t>(sym->slotIndex));
        }
    }
    void visit(adv_node_t* node, Tag<ADV_NODE_FUNCTION_DECL>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_PARAM_DECL>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_IMPORT_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_IF_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_WHILE_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_RETURN_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_YIELD_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_WAIT_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_EXPR_STMT>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_IMPLICIT_CAST_EXPR>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_EXPLICIT_CAST_EXPR>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_IDENTIFIER_EXPR>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_ASSIGN_EXPR>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_BINARY_EXPR>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_UNARY_EXPR>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_CALL_EXPR>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_MEMBER_ACCESS>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_BOOL_LITERAL>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_INT_LITERAL>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_FLOAT_LITERAL>) {}
    void visit(adv_node_t* node, Tag<ADV_NODE_STRING_LITERAL>) {}
    void acceptNode(adv_node_t* node) {
        accept<CodeGenVisitor<E>>(node, std::cref(options), std::ref(ctx), std::ref(buffer));
    }
    const Options& options;
    Context& ctx;
    std::vector<uint8_t>& buffer;
    E emitter{};
};

} // namespace

Generator::Generator(const Options& options, Context& ctx) noexcept : _options(options), _ctx(ctx) {}

adv_result_t Generator::gen() {
    accept<CodeGenVisitor<EmitterText>>(_ctx.root(), std::cref(_options), std::ref(_ctx), std::ref(_buffer));
    return ADV_RESULT_SUCCESS;
}

} // namespace adv
