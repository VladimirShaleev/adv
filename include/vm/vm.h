/**
 * @file      vm.h
 * @brief     API for the virtual machine.
 * @details   Declares VM result codes and functions for managing VM instances
 *            and loading and executing scripts.
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   vm_files
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
 * @ingroup vm_enums
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

/**
 * @brief   Raw bytecode block.
 * @details Represents a contiguous block of compiled VM bytecode in memory.
 *          The buffer may contain either binary bytecode or a textual assembler
 *          representation accepted by the VM loader.
 * @ingroup vm_structs
 */
typedef struct
{
    const vm_char_t* data; /**< Pointer to the bytecode bytes. */
    vm_uint32_t      size; /**< Size of the bytecode block in bytes. */
} vm_bytecode_t;

/**
 * @brief     Native callback invoked by the VM.
 * @details   Called when a registered native function is executed from script
 *            bytecode. The implementation receives the active VM instance, the
 *            calling thread, the native handle, the number of arguments passed,
 *            and any opaque user data associated with the registration.
 * @param[in] vm Active VM instance.
 * @param[in] thread Calling thread context.
 * @param[in] native Native function registration handle.
 * @param[in] arg_count Number of arguments supplied by the caller.
 * @param[in] data User data registered with the native function.
 * @ingroup   vm_callbacks
 */
typedef void
(*vm_native_callback_t)(vm_t vm,
                        vm_thread_h thread,
                        vm_native_h native,
                        vm_uint16_t arg_count,
                        vm_data_t data);

/**
 * @brief     Import callback used to resolve module dependencies.
 * @details   Called when the VM needs to load an external module by name.
 *            The callback must return a pointer to a bytecode block. The VM 
 *            copies the bytecode for its own use while importing the module.
 * @note      If ::vm_release_import_callback_t is set, the VM calls it
 *            immediately after copying the bytecode and importing the module.
 * @param[in] vm Active VM instance.
 * @param[in] module Module identifier to import.
 * @param[in] data User data passed to the importer.
 * @return    Pointer to a loaded bytecode block, or `NULL` when import fails.
 * @see       ::vm_release_import_callback_t
 * @ingroup   vm_callbacks
 */
typedef vm_bytecode_t*
(*vm_import_callback_t)(vm_t vm,
                        vm_utf8_t module,
                        vm_data_t data);

/**
 * @brief     Finalizer for imported bytecode.
 * @details   Releases any memory or resources associated with a bytecode block
 *            returned by a custom import callback.
 * @param[in] vm Active VM instance.
 * @param[in] bytecode Bytecode block to release.
 * @param[in] data User data passed to the importer.
 * @ingroup   vm_callbacks
 */
typedef void
(*vm_release_import_callback_t)(vm_t vm,
                                vm_bytecode_t* bytecode,
                                vm_data_t data);

/**
 * @brief     Converts error code to descriptive string.
 * @details   Provides a text description for the result code.
 * @param[in] result Result code.
 * @return    Corresponding text description of the result code.
 * @ingroup   vm_functions
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
 * @ingroup    vm_functions
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
 * @ingroup   vm_functions
 */
VM_API vm_t
vm_reference(vm_t vm);

/**
 * @brief     Releases a reference to a VM instance.
 * @details   Destroys the instance when its reference count reaches zero.
 *            A null handle is ignored.
 * @param[in] vm VM instance to release.
 * @sa        ::vm_reference
 * @ingroup   vm_functions
 */
VM_API void
vm_destroy(vm_t vm);

/**
 * @brief     Advances the VM execution loop.
 * @details   Processes pending work for the VM, including scheduled ticks and
 *            deferred runtime tasks.
 * @param[in] vm VM instance to advance.
 * @ingroup   vm_functions
 */
VM_API void
vm_tick(vm_t vm);

/**
 * @brief      Loads a script from a file and starts its execution.
 * @details    The input may contain compiled binary data or supported text
 *             script data.
 * @param[in]  vm VM instance that will execute the script.
 * @param[in]  file UTF-8 path to the script file.
 * @return     Handle of the loaded script, or an invalid handle on failure.
 * @ingroup    vm_functions
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
 * @ingroup    vm_functions
 */
VM_API vm_script_h
vm_load_script_from_memory(vm_t vm,
                           const vm_char_t* code,
                           vm_uint32_t code_size);

/**
 * @brief      Creates and starts a VM thread for a script.
 * @details    The returned thread handle begins execution of the provided
 *             script according to the VM runtime scheduling rules.
 * @param[in]  vm VM instance owning the thread.
 * @param[in]  script Script to execute.
 * @return     Thread handle, or an invalid handle on failure.
 * @ingroup    vm_functions
 */
VM_API vm_thread_h
vm_run_thread(vm_t vm,
              vm_script_h script);

/**
 * @brief     Stops and releases a VM thread.
 * @details   Requests shutdown of a thread and releases any resources associated
 *            with its execution context.
 * @param[in] vm VM instance owning the thread.
 * @param[in] thread Thread to shut down.
 * @ingroup   vm_functions
 */
VM_API void
vm_shutdown_thread(vm_t vm,
                   vm_thread_h thread);

/**
 * @brief      Reads an argument passed to the current native call.
 * @details    Native functions may use this helper to read arguments supplied by
 *             the VM for the currently executing thread. The value is returned in
 *             the native representation expected by the host runtime.
 * @param[in]  vm VM instance owning the thread.
 * @param[in]  thread Thread to query.
 * @param[in]  index Argument index.
 * @return     Argument value at @p index.
 * @sa         ::vm_register_native
 * @ingroup    vm_functions
 */
VM_API vm_value_t
vm_thread_get_arg(vm_t vm,
                  vm_thread_h thread,
                  vm_uint32_t index);

/**
 * @brief     Stores a result value for the current native call.
 * @details   Writes one result entry at the specified index for the given
 *            thread execution context. This is the standard way for a native
 *            callback to return values back into the VM.
 * @param[in] vm VM instance owning the thread.
 * @param[in] thread Thread to update.
 * @param[in] index Result slot index.
 * @param[in] value Result value to store.
 * @sa        ::vm_register_native
 * @ingroup   vm_functions
 */
VM_API void
vm_thread_set_result(vm_t vm,
                     vm_thread_h thread,
                     vm_uint32_t index,
                     const vm_value_t* value);

/**
 * @brief      Retrieves a string from the VM string pool.
 * @details    Returns a UTF-8 pointer for the given interned string handle.
 * @param[in]  vm VM instance owning the string.
 * @param[in]  string String handle to resolve.
 * @param[out] length Optional length of the returned string in characters.
 * @return     UTF-8 text for the string, or empty string if the handle is invalid.
 * @ingroup    vm_functions
 */
VM_API vm_utf8_t
vm_get_string(vm_t vm,
              vm_string_h string,
              vm_uint32_t* length);

/**
 * @brief      Interns a UTF-8 string in the VM.
 * @details    Stores the supplied string in the VM string table and returns a
 *             stable handle for later use.
 * @param[in]  vm VM instance owning the string table.
 * @param[in]  string UTF-8 text to intern.
 * @return     Interned string handle.
 * @ingroup    vm_functions
 */
VM_API vm_string_h
vm_intern_string(vm_t vm,
                 vm_utf8_t string);

/**
 * @brief      Resolves the callback for a native function.
 * @details    Returns the registered callback and optional user data pointer
 *             associated with the native registration.
 * @param[in]  vm VM instance owning the native.
 * @param[in]  native Native function handle.
 * @param[out] data Optional user data pointer associated with the native.
 * @return     Pointer to the native callback function, or `NULL` if unavailable.
 * @ingroup    vm_functions
 */
VM_API vm_native_callback_t
vm_get_native(vm_t vm,
              vm_native_h native,
              vm_data_t* data);

/**
 * @brief      Registers a native function with the VM.
 * @details    Creates a callable native entry that can be invoked by script
 *             bytecode. Registered callbacks can read arguments with
 *             ::vm_thread_get_arg and write results with
 *             ::vm_thread_set_result.
 * @param[in]  vm VM instance to register the native with.
 * @param[in]  name Symbol name exposed to scripts.
 * @param[in]  func Callback implementation to invoke.
 * @param[in]  data Optional user data passed to the callback.
 * @return     Registered native handle, or an invalid handle on failure.
 * @sa         ::vm_unregister_native
 * @ingroup    vm_functions
 */
VM_API vm_native_h
vm_register_native(vm_t vm,
                   vm_utf8_t name,
                   vm_native_callback_t func,
                   vm_data_t data);

/**
 * @brief     Unregisters a native function.
 * @details   Releases the registration associated with a native handle.
 * @param[in] vm VM instance owning the native registration.
 * @param[in] native Native handle to unregister.
 * @sa        ::vm_register_native
 * @ingroup   vm_functions
 */
VM_API void
vm_unregister_native(vm_t vm,
                     vm_native_h native);

/**
 * @brief      Retrieves import directories configured for the VM.
 * @details    Copies the active module search paths into the caller-provided
 *             array. The caller is responsible for providing storage for
 *             @p dir_count entries.
 * @note       If @p dirs is `NULL`, the function does not write directory paths
 *             and instead fills @p dir_count with the number of available
 *             entries required to store the full list.
 * @param[in]  vm VM instance to query.
 * @param[out] dir_count Number of directories returned, or the required count
 *             when @p dirs is `NULL`.
 * @param[out] dirs Array of UTF-8 directory paths, or `NULL` to query the
 *             required buffer size.
 * @ingroup    vm_functions
 */
VM_API void
vm_get_import_dirs(vm_t vm,
                   vm_uint32_t* dir_count,
                   vm_utf8_t* dirs);

/**
 * @brief     Replaces the VM import directory list.
 * @details   Updates the module search paths used when resolving imports.
 * @param[in] vm VM instance to update.
 * @param[in] dir_count Number of entries in @p dirs.
 * @param[in] dirs Array of UTF-8 directory paths.
 * @ingroup   vm_functions
 */
VM_API void
vm_set_import_dirs(vm_t vm,
                   vm_uint32_t dir_count,
                   const vm_utf8_t* dirs);

/**
 * @brief      Resolves the active import callback.
 * @details    Returns the current importer callback function and any associated
 *             user data.
 * @param[in]  vm VM instance to query.
 * @param[out] data Optional user data pointer associated with the importer.
 * @return     Current importer callback, or `NULL` when no importer is set.
 * @ingroup    vm_functions
 */
VM_API vm_import_callback_t
vm_get_importer(vm_t vm,
                vm_data_t* data);

/**
 * @brief     Sets the VM module import callback.
 * @details   Configures the importer used when scripts reference external
 *            modules.
 * @param[in] vm VM instance to update.
 * @param[in] callback Import callback implementation.
 * @param[in] data User data passed to the importer.
 * @ingroup   vm_functions
 */
VM_API void
vm_set_importer(vm_t vm,
                vm_import_callback_t callback,
                vm_data_t data);

/**
 * @brief      Resolves the release callback for imported bytecode.
 * @details    Returns the current callback used to clean up bytecode returned
 *             by the import mechanism.
 * @param[in]  vm VM instance to query.
 * @param[out] data Optional user data pointer associated with the release callback.
 * @return     Current release callback, or `NULL` when no callback is set.
 * @ingroup    vm_functions
 */
VM_API vm_release_import_callback_t
vm_get_release_import(vm_t vm,
                      vm_data_t* data);

/**
 * @brief     Sets the release callback used for imported bytecode.
 * @details   Specifies how imported VM bytecode is destroyed after use.
 * @param[in] vm VM instance to update.
 * @param[in] callback Release callback implementation.
 * @param[in] data User data passed to the release callback.
 * @ingroup   vm_functions
 */
VM_API void
vm_set_release_import(vm_t vm,
                      vm_release_import_callback_t callback,
                      vm_data_t data);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* VM_H */
