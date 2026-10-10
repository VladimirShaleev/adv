/**
 * @file      vm.h
 * @brief     API for the virtual machine.
 * @details   Declares VM result codes and functions for managing VM instances
 *            and loading and executing scripts.
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   files
 * @copyright MIT License
 *
 *     MIT License
 *
 *     Copyright (c) 2026 Vladimir Shaleev
 *
 *     Permission is hereby granted, free of charge, to any person obtaining a copy
 *     of this software and associated documentation files (the "Software"), to deal
 *     in the Software without restriction, including without limitation the rights
 *     to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *     copies of the Software, and to permit persons to whom the Software is
 *     furnished to do so, subject to the following conditions:
 *
 *     The above copyright notice and this permission notice shall be included in all
 *     copies or substantial portions of the Software.
 *
 *     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *     IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *     FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *     AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *     LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *     OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *     SOFTWARE.
 *
 */
#ifndef VM_H
#define VM_H

#include "vm-opcode.h"
#include "vm-script.h"
#include "vm-value.h"
#include "vm-version.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Result codes.
 * @details Enumeration of result codes.
 * @ingroup enums
 */
typedef enum
{
    VM_RESULT_SUCCESS             = 0, /**< Indicates success (this is not an error). */
    VM_RESULT_ERROR_UNKNOWN       = 1, /**< Unknown error. */
    VM_RESULT_ERROR_OUT_OF_MEMORY = 2, /**< Out of memory. */
    VM_RESULT_ERROR_INVALID_ARG   = 3, /**< Invalid argument. */
    VM_RESULT_COUNT_ENUMS         = 4, /**< Count of constants in the enumeration. */
    VM_RESULT_MAX_ENUM            = 0x7FFFFFFF /**< Max value of enum (not used) */
} vm_result_t;

typedef struct
{
    const vm_char_t* data;
    vm_uint32_t      size;
} vm_bytecode_t;

typedef void
(*vm_native_callback_t)(vm_t vm,
                        vm_thread_h thread,
                        vm_uint16_t arg_count,
                        vm_data_t data);

typedef vm_bytecode_t*
(*vm_import_callback_t)(vm_t vm,
                        vm_utf8_t module,
                        vm_data_t data);

typedef void
(*vm_release_import_callback_t)(vm_t vm,
                                vm_bytecode_t* bytecode,
                                vm_data_t data);

/**
 * @brief     Converts error code to descriptive string.
 * @details   Provides a text description for the result code.
 * @param[in] result Result code.
 * @return    Corresponding text description of the result code.
 * @ingroup   functions
 */
VM_API vm_utf8_t
vm_result_to_string(vm_result_t result);

/**
 * @name Functions for Virtual Machine.
 * @brief Functions for opaque type ::vm_t.
 * @{
 */

/**
 * @brief      Creates a virtual machine instance.
 * @details    Creates an object for virtual machine.
 * @param[out] vm Receives the newly created instance.
 * @return     ::VM_RESULT_SUCCESS on success, or an error result.
 * @ingroup    functions
 */
VM_API vm_result_t
vm_create(vm_t* vm);

/**
 * @brief     Increments the reference count of a VM instance.
 * @details   Returns the same handle after incrementing its internal reference
 *            count. The instance remains alive until all references are
 *            destroyed.
 * @param[in] vm VM instance to reference.
 * @return    The same VM handle.
 * @sa        ::vm_destroy
 * @ingroup   functions
 */
VM_API vm_t
vm_reference(vm_t vm);

/**
 * @brief     Releases a reference to a VM instance.
 * @details   Destroys the instance when its reference count reaches zero.
 *            A null handle is ignored.
 * @param[in] vm VM instance to release.
 * @sa        ::vm_reference
 * @ingroup   functions
 */
VM_API void
vm_destroy(vm_t vm);

/**
 * @brief      Loads a script from a file and starts its execution.
 * @details    The input may contain compiled binary data or supported text
 *             script data.
 * @param[in]  vm VM instance that will execute the script.
 * @param[in]  file UTF-8 path to the script file.
 * @return     Handle of the loaded script, or an invalid handle on failure.
 * @ingroup    functions
 */
VM_API vm_script_h
vm_load_script_from_file(vm_t vm,
                         vm_utf8_t file);

/**
 * @brief      Loads a script from memory and starts its execution.
 * @details    The input may contain compiled binary data or supported text
 *             script data.
 * @param[in]  vm VM instance that will execute the script.
 * @param[in]  code Script data.
 * @param[in]  code_size Size of @p code in bytes.
 * @return     Handle of the loaded script, or an invalid handle on failure.
 * @ingroup    functions
 */
VM_API vm_script_h
vm_load_script_from_memory(vm_t vm,
                           const vm_char_t* code,
                           vm_uint32_t code_size);

VM_API vm_thread_h
vm_run_script(vm_t vm,
              vm_script_h script);

VM_API void
vm_shutdown_thread(vm_t vm,
                   vm_thread_h thread);

VM_API vm_native_callback_t
vm_get_native(vm_t vm,
              vm_native_h native,
              vm_data_t* data);

VM_API vm_native_h
vm_register_native(vm_t vm,
                   vm_utf8_t name,
                   vm_native_callback_t func,
                   vm_data_t data);

VM_API vm_native_h
vm_unregister_native(vm_t vm,
                     vm_utf8_t name);

VM_API void
vm_get_import_dirs(vm_t vm,
                   vm_uint32_t* dir_count,
                   vm_utf8_t* dirs);

VM_API void
vm_set_import_dirs(vm_t vm,
                   vm_uint32_t dir_count,
                   const vm_utf8_t* dirs);

VM_API vm_import_callback_t
vm_get_importer(vm_t vm,
                vm_data_t* data);

VM_API void
vm_set_importer(vm_t vm,
                vm_import_callback_t callback,
                vm_data_t data);

VM_API vm_release_import_callback_t
vm_get_release_import(vm_t vm,
                      vm_data_t* data);

VM_API void
vm_set_release_import(vm_t vm,
                      vm_release_import_callback_t callback,
                      vm_data_t data);

VM_API void
vm_tick(vm_t vm);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* VM_H */
