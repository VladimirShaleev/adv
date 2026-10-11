/**
 * @file      vm-types.h
 * @brief     Opaque handle types for the virtual machine.
 * @details   Defines strongly-typed opaque handle declarations used across the VM API.
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   files
 * @copyright MIT License
 */
#ifndef VM_TYPES_H
#define VM_TYPES_H

#include "vm-platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Opaque handle to a virtual machine instance.
 * @details The instance representation is hidden from API users.
 * @ingroup types
 */
VM_TYPE(vm)

/**
 * @name    VM Resource Handles
 * @brief   Index-based handles for VM resources.
 * @details Strongly typed handles identify strings and compiled scripts
 *          managed by the virtual machine.
 * @sa      VM_HANDLE
 * @{
 */
VM_HANDLE(vm_string) /**< Handle identifying an interned string in the VM string pool. */
VM_HANDLE(vm_script) /**< Handle identifying a compiled script managed by the VM. */
VM_HANDLE(vm_thread) /**< Handle to a running VM thread. */
VM_HANDLE(vm_native) /**< Handle to a native function entry. */
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* VM_TYPES_H */
