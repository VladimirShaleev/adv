/**
 * @file      vm-types.h
 * @brief     Opaque handle types for the virtual machine.
 * @details   Defines strongly-typed opaque handle declarations used across the VM API.
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   vm_files
 * @copyright MIT License
 */
#ifndef VM_TYPES_H
#define VM_TYPES_H

#include "vm-platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup vm_types
 * @{
 */

/**
 * @name    Opaque Object Types
 * @brief   Forward declarations for VM objects using opaque pointer types
 * @details These macros generate typedefs for pointers to incomplete struct types,
 *          providing type safety while hiding implementation details.
 * @sa      VM_TYPE
 * @{
 */
VM_TYPE(vm) /**< Opaque handle to a virtual machine instance. */
/** @} */

/** 
 * @struct  vm_string_h
 * @brief   Handle of an interned string.
 * @details Handle identifying an interned string in the VM string pool.
 * @var     vm_string_h::handle
 * Handle, contains the value 0 if empty string.
 */
VM_HANDLE(vm_string)

/**
 * @struct  vm_script_h
 * @brief   Handle a compiled script.
 * @details Handle identifying a compiled script managed by the VM.
 * @var     vm_script_h::handle
 * Handle, contains the value 0 if not defined.
 */
VM_HANDLE(vm_script)

/**
 * @struct  vm_thread_h
 * @brief   Handle of a thread.
 * @details Handle to a running VM thread.
 * @var     vm_thread_h::handle
 * Handle, contains the value 0 if not defined.
 */
VM_HANDLE(vm_thread)

/**
 * @struct  vm_native_h
 * @brief   Handle of a native function.
 * @details Handle to a native function entry.
 * @var     vm_native_h::handle
 * Handle, contains the value 0 if not defined.
 */
VM_HANDLE(vm_native)

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* VM_TYPES_H */
