/**
 * @file      adv-symbol.h
 * @brief     Symbol table entry definition.
 * @details   This header defines the structure representing a symbol table entry in the Adv compiler.
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   files
 * @copyright MIT License
 */
#ifndef ADV_SYMBOL_H
#define ADV_SYMBOL_H

#include "adv-ast.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Symbol kinds.
 * @details Identifies the role of a symbol declared in the source code.
 * @ingroup enums
 */
typedef enum
{
    ADV_SYMBOL_KIND_VARIABLE    = 0, /**< Variable symbol. */
    ADV_SYMBOL_KIND_PARAMETER   = 1, /**< Function parameter symbol. */
    ADV_SYMBOL_KIND_NATIVE      = 2, /**< Native symbol. */
    ADV_SYMBOL_KIND_FUNCTION    = 3, /**< Function symbol. */
    ADV_SYMBOL_KIND_STRUCT      = 4, /**< Struct symbol. */
    ADV_SYMBOL_KIND_COUNT_ENUMS = 5, /**< Count of constants in the enumeration. */
    ADV_SYMBOL_KIND_MAX_ENUM    = 0x7FFFFFFF /**< Max value of enum (not used). */
} adv_symbol_kind_t;

/**
 * @brief   Symbol table entry.
 * @details Represents a symbol in the symbol table with its name, kind, type, and other attributes.
 * @ingroup structs
 */
typedef struct
{
    vm_string_h         name; /**< Interned symbol name string handle. */
    adv_symbol_kind_t   kind; /**< Symbol kind (variable, parameter, function, struct). */
    vm_value_type_t     type; /**< Resolved type of the symbol. */
    adv_location_t      location; /**< Source location of the symbol declaration. */
    const adv_symbol_h* params; /**< Array of parameter symbols (for functions). */
    adv_uint16_t        param_count; /**< Number of parameters (for functions). */
    adv_scope_h         scope; /**< Handle to the scope in which the symbol is defined. */
    adv_uint16_t        slot_index; /**< Index of the symbol in the local frame or global table. */
    adv_uint16_t        frame_slot_count; /**< Number of slots used by the symbol (for functions). */
    adv_bool_t          is_global; /**< Flag indicating whether the symbol is globally scoped. */
} adv_symbol_t;

/**
 * @brief   Symbol scope.
 * @details Represents a lexical scope containing symbols and potentially nested child scopes.
 * @ingroup structs
 */
typedef struct
{
    const adv_scope_h*  children;  /**< Pointer to the first child scope (nullptr if no children). */
    const adv_symbol_h* symbols; /**< Pointer to the first symbol in this scope. */
    adv_uint16_t        child_count; /**< Number of child scopes. */
    adv_uint16_t        symbol_count; /**< Number of symbols in this scope. */
    adv_scope_h         parent; /**< Handle to the parent scope. */
} adv_scope_t;

#ifdef __cplusplus
}
#endif

#endif /* ADV_SYMBOL_H */
