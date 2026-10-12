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

#include "adv-types.h"

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
    ADV_NODE_UNKNOWN            = 0, /**< Unknown or uninitialized AST node type. */

    /* Top-level and statements */
    ADV_NODE_PROGRAM            = 1, /**< Root node: stmts.first_stmt -> list of top-level statements. */
    ADV_NODE_BLOCK              = 2, /**< Block `{ ... }`: stmts.first_stmt -> list of block statements. */

    /* Declarations */
    ADV_NODE_VAR_DECL           = 3, /**< `var x: type = expr`: var_decl. */
    ADV_NODE_NATIVE_DECL        = 4, /**< `native name(...)`: func_decl. */
    ADV_NODE_FUNCTION_DECL      = 5, /**< `function name(...)`: func_decl. */
    ADV_NODE_PARAM_DECL         = 6, /**< `a: int`: param_decl (used in func_decl). */

    /* Control flow statements */
    ADV_NODE_IMPORT_STMT        = 7, /**< `import "file"`: import_stmt (path). */
    ADV_NODE_IF_STMT            = 8, /**< `if (cond) then else`: if_stmt. */
    ADV_NODE_WHILE_STMT         = 9, /**< `while (cond) body`: while_stmt. */
    ADV_NODE_RETURN_STMT        = 10, /**< `return expr`: expr_stmt. */
    ADV_NODE_YIELD_STMT         = 11, /**< `yield`: yield statement (no additional union payload). */
    ADV_NODE_WAIT_STMT          = 12, /**< `wait expr`: expr_stmt. */
    ADV_NODE_EXPR_STMT          = 13, /**< Expression statement: expr_stmt. */

    /* Expressions and operations */
    ADV_NODE_IMPLICIT_CAST_EXPR = 14, /**< Implicit type conversion node inserted during semantic analysis. */
    ADV_NODE_EXPLICIT_CAST_EXPR = 15, /**< `expr AS type`: explicit cast_expr. */
    ADV_NODE_IDENTIFIER_EXPR    = 16, /**< Identifier reference expression: id_expr. */
    ADV_NODE_ASSIGN_EXPR        = 17, /**< `target = val`: assign_expr. */
    ADV_NODE_BINARY_EXPR        = 18, /**< `a + b`, `a == b`: binary_expr. */
    ADV_NODE_UNARY_EXPR         = 19, /**< `-a`, `!a`: unary_expr. */
    ADV_NODE_CALL_EXPR          = 20, /**< `func(a, b)` or `coroutine[a, b]`: call_expr. */
    ADV_NODE_MEMBER_ACCESS      = 21, /**< `obj.field`: member_access. */

    /* Literals */
    ADV_NODE_BOOL_LITERAL       = 22, /**< Boolean literal: bool_val. */
    ADV_NODE_INT_LITERAL        = 23, /**< 32-bit signed integer literal: int_val. */
    ADV_NODE_FLOAT_LITERAL      = 24, /**< 32-bit floating-point literal: float_val. */
    ADV_NODE_STRING_LITERAL     = 25, /**< String literal handle: str_val. */

    ADV_NODE_COUNT_ENUMS        = 26, /**< Count of constants in the enumeration. */
    ADV_NODE_MAX_ENUM           = 0x7FFFFFFF /**< Max value of enum (not used). */
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

/**
 * @brief   Forward declaration of an AST node.
 * @details The node definition follows below; pointers to this type are used
 *          to link nodes into the AST hierarchy.
 * @ingroup types
 */
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
        struct
        {
            adv_node_t*  first_stmt; /**< Head of statement nodes list. */
            adv_uint32_t stmt_count; /**< Number of statements in list. */
        } stmts; /**< Statement list payload. */

        struct
        {
            vm_string_h  name;      /**< Variable name string handle. */
            vm_string_h  type_hint; /**< Type annotation handle (0 if omitted). */
            adv_node_t*  init_expr; /**< Initializer expression node (nullptr if uninitialized). */
            adv_bool_t   is_global; /**< Flag indicating whether variable is globally scoped. */
            adv_symbol_h symbol;    /**< Handle to associated symbol. */
        } var_decl; /**< Variable declaration payload. */

        struct
        {
            vm_string_h  name;             /**< Function name string handle. */
            vm_string_h  return_type_hint; /**< Return type annotation handle. */
            adv_node_t*  first_param;      /**< Head of parameter declarations list (linked by next_sibling). */
            adv_uint32_t param_count;      /**< Number of parameters. */
            adv_node_t*  body;             /**< Pointer to BLOCK node. */
            adv_symbol_h symbol;           /**< Handle to associated symbol. */
        } func_decl; /**< Function declaration payload. */

        struct
        {
            vm_string_h name;      /**< Parameter name string handle. */
            vm_string_h type_hint; /**< Type annotation handle (0 if omitted). */
            adv_symbol_h symbol;   /**< Handle to associated symbol. */
        } param_decl; /**< Function parameter declaration payload. */

        struct
        {
            adv_node_t* root; /**< Root node of imported AST. */
            vm_string_h path; /**< Imported module or file path string handle. */
        } import_stmt; /**< Import statement payload. */

        struct
        {
            adv_node_t* cond;        /**< Condition expression. */
            adv_node_t* then_branch; /**< Execution block if condition evaluates to true. */
            adv_node_t* else_branch; /**< Optional else block or nested ADV_NODE_IF_STMT. */
        } if_stmt; /**< Conditional statement payload. */

        struct
        {
            adv_node_t* cond; /**< Loop condition expression. */
            adv_node_t* body; /**< Pointer to BLOCK node executed per iteration. */
        } while_stmt; /**< Loop statement payload. */

        struct
        {
            adv_node_t* expr; /**< Expression payload (nullptr if omitted/void). */
        } expr_stmt; /**< Expression statement payload. */

        struct
        {
            adv_node_t*     operand;          /**< Expression being converted. */
            vm_string_h     target_type_hint; /**< Explicit target type string handle. */
            vm_value_type_t from_type;        /**< Original value type. */
            vm_value_type_t to_type;          /**< Target converted type. */
        } cast_expr; /**< Type cast expression payload. */

        struct
        {
            vm_string_h  name;      /**< Identifier name string handle. */
            vm_string_h  type_hint; /**< Type annotation handle (0 if omitted). */
            adv_symbol_h symbol;    /**< Handle to associated symbol. */
        } id_expr; /**< Identifier reference expression payload. */

        struct
        {
            adv_node_t* target;     /**< Target node (IDENTIFIER_EXPR or MEMBER_ACCESS). */
            adv_node_t* val;        /**< Value expression to assign. */
        } assign_expr; /**< Assignment expression payload. */

        struct
        {
            adv_uint32_t op;        /**< Operator token code (e.g. '+', EQ). */
            adv_node_t*  left;      /**< Left-hand operand. */
            adv_node_t*  right;     /**< Right-hand operand. */
        } binary_expr; /**< Binary operator expression payload. */

        struct
        {
            adv_uint32_t op;        /**< Operator token code. */
            adv_node_t*  operand;   /**< Target operand. */
        } unary_expr; /**< Unary operator expression payload. */

        struct
        {
            vm_string_h  name;      /**< Function or coroutine name string handle. */
            adv_node_t*  first_arg; /**< Head of argument expressions list (linked by next_sibling). */
            adv_uint32_t arg_count; /**< Number of arguments passed. */
            adv_bool_t   is_async;  /**< True for asynchronous invocation `coroutine[a, b]`, false for standard `func(a, b)`. */
            adv_symbol_h symbol;    /**< Handle to associated symbol. */
        } call_expr; /**< Call expression payload. */

        struct
        {
            adv_node_t* target;     /**< Target object instance expression. */
            vm_string_h member;     /**< Member field name string handle. */
        } member_access; /**< Member access expression payload. */

        adv_bool_t    bool_val;    /**< Boolean literal value. */
        adv_sint32_t  int_val;     /**< Integer literal value. */
        adv_float32_t float_val;   /**< Floating-point literal value. */
        vm_string_h   str_val;     /**< String literal handle. */
    };
};

#ifdef __cplusplus
}
#endif

#endif /* ADV_AST_H */
