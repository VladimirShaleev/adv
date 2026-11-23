/**
 * @file      adv-symbol.h
 * @brief     Symbol table entry definition.
 * @details   This header defines the structure representing a symbol table entry in the Adv compiler.
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   adv_files
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
 * @ingroup adv_enums
 */
typedef enum
{
    ADV_SYMBOL_KIND_PRIMITIVE   = 0, /**< Primitive type symbol (e.g., bool, int, float, string). */
    ADV_SYMBOL_KIND_VOID        = 1, /**< Void type symbol. */
    ADV_SYMBOL_KIND_VARIABLE    = 2, /**< Variable symbol. */
    ADV_SYMBOL_KIND_PARAMETER   = 3, /**< Function parameter symbol. */
    ADV_SYMBOL_KIND_NATIVE      = 4, /**< Native symbol. */
    ADV_SYMBOL_KIND_FUNCTION    = 5, /**< Function symbol. */
    ADV_SYMBOL_KIND_FIELD       = 6, /**< Field symbol. */
    ADV_SYMBOL_KIND_STRUCTURE   = 7, /**< Struct symbol. */
    ADV_SYMBOL_KIND_COUNT_ENUMS = 8, /**< Count of constants in the enumeration. */
    ADV_SYMBOL_KIND_MAX_ENUM    = 0x7FFFFFFF /**< Max value of enum (not used). */
} adv_symbol_kind_t;

/**
 * @brief   Symbol table entry.
 * @details Represents a symbol in the symbol table with its name, kind, type, and other attributes.
 * @ingroup adv_structs
 */
typedef struct
{
    vm_string_h         name; /**< Interned symbol name string handle. */
    adv_symbol_kind_t   kind; /**< Symbol kind (variable, parameter, function, field, structure). */
    vm_value_type_t     primitive_type; /**< Primitive type of the symbol (for primitive symbols). */
    adv_symbol_h        type; /**< Resolved type of the symbol. */
    adv_location_t      location; /**< Source location of the symbol declaration. */
    const adv_symbol_h* children; /**< Array of child symbols (function parameters or structure fields). */
    adv_uint16_t        child_count; /**< Number of child symbols. */
    adv_scope_h         scope; /**< Handle to the scope in which the symbol is defined. */
    adv_uint16_t        slot_index; /**< Index of the symbol in the local frame or global table. */
    adv_uint16_t        slot_count; /**< Number of slots used by the symbol. */
    adv_bool_t          is_global; /**< Flag indicating whether the symbol is globally scoped. */
} adv_symbol_t;

/**
 * @brief   Symbol scope.
 * @details Represents a lexical scope containing symbols and potentially nested child scopes.
 * @ingroup adv_structs
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
