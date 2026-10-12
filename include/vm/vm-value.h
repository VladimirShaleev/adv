/**
 * @file      vm-value.h
 * @brief     Value representations and type definitions for the virtual machine.
 * @details   Defines the primitive types, string handle wrappers, and tagged union values used by the VM runtime.
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   files
 * @copyright MIT License
 */
#ifndef VM_VALUE_H
#define VM_VALUE_H

#include "vm-types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Value type identifiers.
 * @details Enumeration of primitive data types supported by the virtual machine.
 * @ingroup enums
 */
typedef enum
{
    VM_VALUE_TYPE_UNKNOWN     = 0, /**< Unknown or unresolved value type. */
    VM_VALUE_TYPE_BOOL        = 1, /**< Boolean value type. */
    VM_VALUE_TYPE_INT         = 2, /**< 32-bit signed integer value type. */
    VM_VALUE_TYPE_FLOAT       = 3, /**< 32-bit single-precision floating-point value type. */
    VM_VALUE_TYPE_STRING      = 4, /**< String handle value type. */
    VM_VALUE_TYPE_COUNT_ENUMS = 5, /**< Count of constants in the enumeration. */
    VM_VALUE_TYPE_MAX_ENUM    = 0x7FFFFFFF /**< Max value of enum (not used). */
} vm_value_type_t;

/**
 * @brief   Tagged union representing a VM value.
 * @details Holds both the type tag and the actual payload data.
 * @ingroup structs
 */
typedef struct
{
    vm_value_type_t type; /**< Type identifier of the stored value. */
    union
    {
        vm_bool_t    b;   /**< Boolean payload representation. */
        vm_sint32_t  i32; /**< Signed 32-bit integer payload representation. */
        vm_float32_t f32; /**< 32-bit floating-point payload representation. */
        vm_string_h  str; /**< String handle payload representation. */
    }
#ifdef DOXYGEN
    val; /**< Value payload. */
#endif
    ;
} vm_value_t;

#ifdef __cplusplus
}
#endif

#endif /* VM_VALUE_H */
