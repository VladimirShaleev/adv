#ifndef ADV_VISITOR_HPP
#define ADV_VISITOR_HPP

#include "compiler.hpp"

namespace adv {

template <adv_node_type_t Type>
struct Tag {
    static constexpr adv_node_type_t type = Type;
};

template <typename Visitor, typename... Args>
Visitor accept(adv_node_t* node, Args&&... args) {
    Visitor visitor(std::forward<Args>(args)...);
    if (node) {
        switch (node->type) {
        case ADV_NODE_PROGRAM:
            visitor.visit(node, Tag<ADV_NODE_PROGRAM>{});
            break;
        case ADV_NODE_BLOCK:
            visitor.visit(node, Tag<ADV_NODE_BLOCK>{});
            break;
        case ADV_NODE_VAR_DECL:
            visitor.visit(node, Tag<ADV_NODE_VAR_DECL>{});
            break;
        case ADV_NODE_FUNCTION_DECL:
            visitor.visit(node, Tag<ADV_NODE_FUNCTION_DECL>{});
            break;
        case ADV_NODE_PARAM_DECL:
            visitor.visit(node, Tag<ADV_NODE_PARAM_DECL>{});
            break;
        case ADV_NODE_IMPORT_STMT:
            visitor.visit(node, Tag<ADV_NODE_IMPORT_STMT>{});
            break;
        case ADV_NODE_IF_STMT:
            visitor.visit(node, Tag<ADV_NODE_IF_STMT>{});
            break;
        case ADV_NODE_WHILE_STMT:
            visitor.visit(node, Tag<ADV_NODE_WHILE_STMT>{});
            break;
        case ADV_NODE_RETURN_STMT:
            visitor.visit(node, Tag<ADV_NODE_RETURN_STMT>{});
            break;
        case ADV_NODE_YIELD_STMT:
            visitor.visit(node, Tag<ADV_NODE_YIELD_STMT>{});
            break;
        case ADV_NODE_WAIT_STMT:
            visitor.visit(node, Tag<ADV_NODE_WAIT_STMT>{});
            break;
        case ADV_NODE_EXPR_STMT:
            visitor.visit(node, Tag<ADV_NODE_EXPR_STMT>{});
            break;
        case ADV_NODE_IMPLICIT_CAST_EXPR:
            visitor.visit(node, Tag<ADV_NODE_IMPLICIT_CAST_EXPR>{});
            break;
        case ADV_NODE_EXPLICIT_CAST_EXPR:
            visitor.visit(node, Tag<ADV_NODE_EXPLICIT_CAST_EXPR>{});
            break;
        case ADV_NODE_IDENTIFIER_EXPR:
            visitor.visit(node, Tag<ADV_NODE_IDENTIFIER_EXPR>{});
            break;
        case ADV_NODE_ASSIGN_EXPR:
            visitor.visit(node, Tag<ADV_NODE_ASSIGN_EXPR>{});
            break;
        case ADV_NODE_BINARY_EXPR:
            visitor.visit(node, Tag<ADV_NODE_BINARY_EXPR>{});
            break;
        case ADV_NODE_UNARY_EXPR:
            visitor.visit(node, Tag<ADV_NODE_UNARY_EXPR>{});
            break;
        case ADV_NODE_CALL_EXPR:
            visitor.visit(node, Tag<ADV_NODE_CALL_EXPR>{});
            break;
        case ADV_NODE_MEMBER_ACCESS:
            visitor.visit(node, Tag<ADV_NODE_MEMBER_ACCESS>{});
            break;
        case ADV_NODE_BOOL_LITERAL:
            visitor.visit(node, Tag<ADV_NODE_BOOL_LITERAL>{});
            break;
        case ADV_NODE_INT_LITERAL:
            visitor.visit(node, Tag<ADV_NODE_INT_LITERAL>{});
            break;
        case ADV_NODE_FLOAT_LITERAL:
            visitor.visit(node, Tag<ADV_NODE_FLOAT_LITERAL>{});
            break;
        case ADV_NODE_STRING_LITERAL:
            visitor.visit(node, Tag<ADV_NODE_STRING_LITERAL>{});
            break;
        default:
            assert(!"unreachable code");
            break;
        }
    }
    return visitor;
}

} // namespace adv

#endif // ADV_VISITOR_HPP
