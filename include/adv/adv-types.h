/**
 * @file      adv-types.h
 * @brief     Core type definitions for the Adv compilator.
 * @details   This header defines the fundamental object types and handles used throughout
 *            the Adv compilator. It provides forward declarations for all major system
 *            components using opaque pointer types (#ADV_TYPE).
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   adv_files
 * @copyright MIT License
 */
#ifndef ADV_TYPES_H
#define ADV_TYPES_H

#include "adv-platform.h"

#include <vm/vm.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup adv_types
 * @{
 */

/**
 * @name    Opaque Object Types
 * @brief   Forward declarations for compiler objects using opaque pointer types
 * @details These macros generate typedefs for pointers to incomplete struct types,
 *          providing type safety while hiding implementation details.
 * @sa      ADV_TYPE
 * @{
 */
ADV_TYPE(adv_compilation_result) /**< Compilation result. */
ADV_TYPE(adv_options)            /**< Compilation options. */
ADV_TYPE(adv_compiler)           /**< Compiler interface. */
/** @} */

/** 
 * @struct  adv_symbol_h
 * @brief   Handle of a symbol.
 * @details Handle identifying a symbol in the symbol table.
 * @var     adv_symbol_h::handle
 * Handle, contains the value 0 if not defined.
 */
ADV_HANDLE(adv_symbol)

/** 
 * @struct  adv_scope_h
 * @brief   Handle of a scope.
 * @details Handle identifying a scope in the symbol table.
 * @var     adv_scope_h::handle
 * Handle, contains the value 0 if not defined.
 */
ADV_HANDLE(adv_scope)

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ADV_TYPES_H */
