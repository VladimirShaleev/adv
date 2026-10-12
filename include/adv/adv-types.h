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
 *          providing type safety while hiding implementation details. Each represents
 *          a major subsystem in the Adv compiler.
 * @sa      ADV_TYPE
 * @{
 */
ADV_TYPE(adv_compilation_result) /**< Compilation result. */
ADV_TYPE(adv_options)            /**< Compilation options. */
ADV_TYPE(adv_compiler)           /**< Compiler interface. */
/** @} */

/**
 * @name    ADV Resource Handles
 * @brief   Index-based handles for ADV compiler resources.
 * @details Strongly typed handles identify symbols and lexical scopes in a
 *          compilation result.
 * @sa      ADV_HANDLE
 * @{
 */
ADV_HANDLE(adv_symbol) /**< Handle identifying a symbol in the symbol table. */
ADV_HANDLE(adv_scope)  /**< Handle identifying a scope in the symbol table. */
/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ADV_TYPES_H */
