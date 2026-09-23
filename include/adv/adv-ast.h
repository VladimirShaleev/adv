/**
 * @file      adv-ast.h
 * @brief     Abstract Syntax Tree (AST) node definitions.
 * @details   Defines the node types, source locations, and AST node structure used by the compiler frontend.
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   files
 * @copyright MIT License
 */
#ifndef ADV_AST_H
#define ADV_AST_H

#include "adv-platform.h"

#include <vm/vm.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   AST node type identifiers.
 * @details Enumeration of all supported Abstract Syntax Tree node kinds.
 * @ingroup enums
 */
typedef enum
{
    ADV_NODE_UNKNOWN = 0, /**< Unknown or uninitialized AST node type. */

    /* Top-level and statements */
    ADV_NODE_PROGRAM,            /**< Root node: children.first_child -> list of top-level statements. */
    ADV_NODE_BLOCK,              /**< Block `{ ... }`: children.first_child -> list of statements. */
    ADV_NODE_IMPORT_STMT,        /**< `import "file"`: import_stmt (path). */

    /* Declarations */
    ADV_NODE_VAR_DECL,           /**< `var x: type = expr`: var_decl. */
    ADV_NODE_PARAM_DECL,         /**< `a: int`: param_decl (used in fn_decl). */
    ADV_NODE_FUNCTION_DECL,      /**< `function name(...)`: fn_decl. */

    /* Control flow statements */
    ADV_NODE_IF_STMT,            /**< `if (cond) then else`: if_stmt. */
    ADV_NODE_WHILE_STMT,         /**< `while (cond) body`: while_stmt. */
    ADV_NODE_RETURN_STMT,        /**< `return expr`: unary (or children.first_child). */
    ADV_NODE_YIELD_STMT,         /**< `yield`: yield statement (no additional union payload). */
    ADV_NODE_WAIT_STMT,          /**< `wait expr`: wait statement. */
    ADV_NODE_EXPR_STMT,          /**< Expression statement: children.first_child -> expr. */

    /* Expressions and operations */
    ADV_NODE_ASSIGN_EXPR,        /**< `target = val`: assign. */
    ADV_NODE_BINARY_EXPR,        /**< `a + b`, `a == b`: binop. */
    ADV_NODE_UNARY_EXPR,         /**< `-a`, `!a`: unary. */
    ADV_NODE_CALL_EXPR,          /**< `func(a, b)` or `coroutine[a, b]`: call. */
    ADV_NODE_MEMBER_ACCESS,      /**< `obj.field`: member_access. */

    /* Literals and identifiers */
    ADV_NODE_IDENTIFIER_EXPR,    /**< Identifier reference: id_expr. */
    ADV_NODE_INT_LITERAL,        /**< 32-bit signed integer literal: int_val. */
    ADV_NODE_FLOAT_LITERAL,      /**< 32-bit floating-point literal: float_val. */
    ADV_NODE_STRING_LITERAL,     /**< String literal handle: str_val. */
    ADV_NODE_BOOL_LITERAL,       /**< Boolean literal: bool_val. */

    /* Type casting */
    ADV_NODE_IMPLICIT_CAST_EXPR, /**< Implicit type conversion node inserted during semantic analysis. */
    ADV_NODE_EXPLICIT_CAST_EXPR, /**< `expr AS type`: explicit cast_expr. */

    ADV_NODE_COUNT_ENUMS,        /**< Count of constants in the enumeration. */
    ADV_NODE_MAX_ENUM = 0x7FFFFFFF /**< Max value of enum (not used). */
} adv_node_type_t;

/**
 * @brief   Source location info.
 * @details Stores filename handle, line, and column position in the source code.
 * @ingroup structs
 */
typedef struct
{
    vm_string_h  filename; /**< Interned filename string handle. */
    adv_uint32_t line;     /**< 1-based line number. */
    adv_uint32_t column;   /**< 1-based column number. */
} adv_location_t;

typedef struct adv_node_t adv_node_t;

/**
 * @brief   Abstract Syntax Tree node.
 * @details Represents a generic node in the AST with type tag, source location, resolved semantic type, 
 *          tree hierarchy links, and type-specific union payload.
 * @ingroup structs
 */
struct adv_node_t
{
    adv_node_type_t type;          /**< Type identifier of the AST node. */
    adv_location_t  location;      /**< Source location of the node. */
    vm_value_type_t resolved_type; /**< Type resolved during semantic pass. */
    adv_node_t*     parent;        /**< Pointer to parent AST node. */
    adv_node_t*     next_sibling;  /**< Pointer to next sibling AST node. */

    union
    {
        adv_sint32_t  int_val;     /**< Integer literal value. */
        adv_float32_t float_val;   /**< Floating-point literal value. */
        adv_bool_t    bool_val;    /**< Boolean literal value. */
        vm_string_h   str_val;     /**< String literal handle. */

        /**
         * @brief Generic list of child nodes.
         */
        struct
        {
            adv_node_t*  first_child; /**< Head of child nodes list. */
            adv_uint32_t child_count; /**< Number of children. */
        } children;

        /**
         * @brief Import statement payload.
         */
        struct
        {
            adv_node_t* root; /**< Root node of imported AST. */
            vm_string_h path; /**< Imported module or file path string handle. */
        } import_stmt;

        /**
         * @brief Variable declaration payload.
         */
        struct
        {
            vm_string_h name;      /**< Variable name string handle. */
            vm_string_h type_hint; /**< Type annotation handle (0 if omitted). */
            adv_node_t* init_expr; /**< Initializer expression node (nullptr if uninitialized). */
            adv_bool_t  is_global; /**< Flag indicating whether variable is globally scoped. */
            void*       scope;     /**< Pointer to associated symbol scope. */
        } var_decl;

        /**
         * @brief Identifier expression payload.
         */
        struct
        {
            vm_string_h name;      /**< Identifier name string handle. */
            vm_string_h type_hint; /**< Type annotation handle (0 if omitted). */
            void*       scope;     /**< Pointer to resolved scope context. */
        } id_expr;

        /**
         * @brief Function parameter declaration payload.
         */
        struct
        {
            vm_string_h name;      /**< Parameter name string handle. */
            vm_string_h type_hint; /**< Type annotation handle (0 if omitted). */
            void*       scope;     /**< Pointer to function parameter scope. */
        } param_decl;

        /**
         * @brief Function declaration payload.
         */
        struct
        {
            vm_string_h  name;             /**< Function name string handle. */
            vm_string_h  return_type_hint; /**< Return type annotation handle. */
            adv_node_t*  first_param;      /**< Head of parameter declarations list (linked by next_sibling). */
            adv_uint32_t param_count;      /**< Number of parameters. */
            adv_node_t*  body;             /**< Pointer to BLOCK node. */
            void*        scope;            /**< Pointer to function scope. */
        } fn_decl;

        /**
         * @brief Conditional statement payload.
         */
        struct
        {
            adv_node_t* cond;        /**< Condition expression. */
            adv_node_t* then_branch; /**< Execution block if condition evaluates to true. */
            adv_node_t* else_branch; /**< Optional else block or nested IF_STMT. */
        } if_stmt;

        /**
         * @brief Loop statement payload.
         */
        struct
        {
            adv_node_t* cond; /**< Loop condition expression. */
            adv_node_t* body; /**< Pointer to BLOCK node executed per iteration. */
        } while_stmt;

        /**
         * @brief Wait statement payload.
         */
        struct
        {
            adv_node_t* expr; /**< Expression to wait on (nullptr for frame pause). */
        } wait_stmt;

        /**
         * @brief Binary operator expression payload.
         */
        struct
        {
            adv_uint32_t op;    /**< Operator token code (e.g. '+', EQ). */
            adv_node_t*  left;  /**< Left-hand operand. */
            adv_node_t*  right; /**< Right-hand operand. */
        } binop;

        /**
         * @brief Unary operator expression payload.
         */
        struct
        {
            adv_uint32_t op;      /**< Operator token code. */
            adv_node_t*  operand; /**< Target operand. */
        } unary;

        /**
         * @brief Assignment expression payload.
         */
        struct
        {
            adv_node_t* target; /**< Target node (IDENTIFIER_EXPR or MEMBER_ACCESS). */
            adv_node_t* val;    /**< Value expression to assign. */
        } assign;

        /**
         * @brief Member access expression payload.
         */
        struct
        {
            adv_node_t* target; /**< Target object instance expression. */
            vm_string_h member; /**< Member field name string handle. */
        } member_access;
        
        /**
         * @brief Call expression payload.
         */
        struct
        {
            vm_string_h  name;      /**< Function or coroutine name string handle. */
            adv_node_t*  first_arg; /**< Head of argument expressions list (linked by next_sibling). */
            adv_uint32_t arg_count; /**< Number of arguments passed. */
            adv_bool_t   is_async;  /**< True for asynchronous invocation `coroutine[a, b]`, false for standard `func(a, b)`. */
        } call;

        /**
         * @brief Type cast expression payload.
         */
        struct
        {
            adv_node_t*     operand;          /**< Expression being converted. */
            vm_string_h     target_type_hint; /**< Explicit target type string handle. */
            vm_value_type_t from_type;        /**< Original value type. */
            vm_value_type_t to_type;          /**< Target converted type. */
        } cast_expr;
    };
};

#ifdef __cplusplus
}
#endif

#endif /* ADV_AST_H */
