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

VM_TYPE(vm)

/**
 * @name    String Handles
 * @brief   Opaque handles for interned string references.
 * @details Group of strongly-typed handle declarations used to reference string instances stored in the runtime string pool.
 * @sa      VM_HANDLE
 * @{
 */
VM_HANDLE(vm_string) /**< String handle representation. Unique identifier for the interned string */
VM_HANDLE(vm_script)
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* VM_TYPES_H */
