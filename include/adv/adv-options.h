/**
 * @file      adv-options.h
 * @brief     Compiler options.
 * @details   This is where the structures and various compilation options are located.
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   files
 * @copyright MIT License
 */
#ifndef ADV_OPTIONS_H
#define ADV_OPTIONS_H

#include "adv-result.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Script text or binary format type.
 * @details Defines the representation format of the script code, 
 *          distinguishing between human-readable text assembly and compiled binary bytecode.
 * @ingroup enums
 */
typedef enum
{
    ADV_FORMAT_BINARY      = 0, /**< Binary bytecode format. */
    ADV_FORMAT_TEXT        = 1, /**< Textual assembly source format. */
    ADV_FORMAT_COUNT_ENUMS = 2, /**< Count of constants in the enumeration. */
    ADV_FORMAT_MAX_ENUM    = 0x7FFFFFFF /**< Max value of enum (not used). */
} adv_format_t;

/**
 * @brief   Source code.
 * @details Used to provide source code in memory.
 * @ingroup structs
 */
typedef struct
{
    adv_utf8_t        name; /**< Source name (used to resolve imports). */
    const adv_char_t* data; /**< Source code. */
    adv_uint32_t      size; /**< Size of ::adv_source_t::data in bytes. */
} adv_source_t;

/**
 * @name Function pointer types.
 * @brief Function pointers definitions.
 * @{
 */

/**
 * @brief     Callback to get sources.
 * @details   Used to retrieve and compile sources from memory.
 * @param[in] name The name of the file that the compiler is trying to get (for example, when it encounters "import").
 * @param[in] depth Current imports nesting level.
 * @param[in] data User data specified when setting up a callback.
 * @return    Should return the source if the file can be resolved, or null to indicate
 *            to the compiler that it cannot resolve the source and should try to find
 *            the source elsewhere (e.g. via import paths).
 * @sa        If the callback allocates memory, then you can free it in the callback adv_release_import_callback_t.
 * @ingroup   types
 */
typedef adv_source_t*
(*adv_import_callback_t)(adv_utf8_t name,
                         adv_uint32_t depth,
                         adv_data_t data);

/**
 * @brief     Callback to release sources.
 * @details   If adv_import_callback_t allocated memory dynamically for the source, you can free it here.
 * @param[in] source Source for release.
 * @param[in] data User data specified when setting up a callback.
 * @sa        adv_import_callback_t.
 * @ingroup   types
 */
typedef void
(*adv_release_import_callback_t)(adv_source_t* source,
                                 adv_data_t data);

/**
 * @brief     Callback to which the compilation result is passed.
 * @details   If you need to save the compilation result to a location other than the file
 *            system, such as the network or console output, you can use this callback.
 * @param[in] source Source of compiler output.
 * @param[in] data User data specified when setting up a callback.
 * @ingroup   types
 */
typedef void
(*adv_write_callback_t)(const adv_source_t* source,
                        adv_data_t data);

/** @} */

/**
 * @name Functions of Options.
 * @brief Functions for opaque type ::adv_options_t.
 * @{
 */

/**
 * @brief      Creates new options instance.
 * @details    Creates an object for setting compiler options.
 * @param[out] options New options instance.
 * @return     New options instance.
 * @ingroup    functions
 */
ADV_API adv_result_t
adv_options_create(adv_options_t* options);

/**
 * @brief     Increments reference count.
 * @details   Manages options instance lifetime.
 * @param[in] options Target options instance.
 * @return    Reference to same options.
 * @sa        ::adv_options_destroy
 * @ingroup   functions
 */
ADV_API adv_options_t
adv_options_reference(adv_options_t options);

/**
 * @brief     Releases options instance.
 * @details   Destroys when reference count reaches zero.
 * @param[in] options Options to destroy.
 * @sa        ::adv_options_reference
 * @ingroup   functions
 */
ADV_API void
adv_options_destroy(adv_options_t options);

/**
 * @brief     Get warning handling setting.
 * @details   Return *TRUE* if warnings are treated as errors.
 * @param[in] options Target options.
 * @return    *TRUE* is enabled.
 * @sa        ::adv_options_set_warnings_as_errors
 * @ingroup   functions
 */
ADV_API adv_bool_t
adv_options_get_warnings_as_errors(adv_options_t options);

/**
 * @brief     Set warning handling setting.
 * @details   Setting treat warnings as errors.
 * @param[in] options Target options.
 * @param[in] enable Enable treat warnings as errors.
 * @sa        ::adv_options_get_warnings_as_errors
 * @ingroup   functions
 */
ADV_API void
adv_options_set_warnings_as_errors(adv_options_t options,
                                   adv_bool_t enable);

/**
 * @brief     Get the target output format setting.
 * @details   Return the current output format configured in the options.
 * @param[in] options Target options.
 * @return    Current output format (::adv_format_t).
 * @sa        ::adv_options_set_output_format
 * @ingroup   functions
 */
ADV_API adv_format_t
adv_options_get_output_format(adv_options_t options);

/**
 * @brief     Set the target output format setting.
 * @details   Set the desired output format type for compilation or translation.
 * @param[in] options Target options.
 * @param[in] format Output format type to set.
 * @sa        ::adv_options_get_output_format
 * @ingroup   functions
 */
ADV_API void
adv_options_set_output_format(adv_options_t options,
                              adv_format_t format);

/**
 * @brief     Get output path.
 * @details   Returns the directory or file path that the compiler will use to save compilation output.
 * @param[in] options Target options.
 * @return    Output directory or file path.
 * @sa        ::adv_options_set_output_path
 * @ingroup   functions
 */
ADV_API adv_utf8_t
adv_options_get_output_path(adv_options_t options);

/**
 * @brief     Set output path.
 * @details   Configure the directory or file path that the compiler will use to save compilation output.
 * @param[in] options Target options.
 * @param[in] path Output directory or file path.
 * @note      Compiler output to the file system does not occur if output via a ::adv_options_set_writer is configured.
 * @sa        ::adv_options_get_output_path
 * @ingroup   functions
 */
ADV_API void
adv_options_set_output_path(adv_options_t options,
                            adv_utf8_t path);

/**
 * @brief         Returns an array of directories to search for imports.
 * @details       These paths are used to search source code when an import is encountered during compilation.
 * @param[in]     options Target options.
 * @param[in,out] dir_count Number of directories.
 * @param[out]    dirs Import directories.
 * @sa            ::adv_options_set_import_dirs
 * @ingroup       functions
 */
ADV_API void
adv_options_get_import_dirs(adv_options_t options,
                            adv_uint32_t* dir_count,
                            adv_utf8_t* dirs);

/**
 * @brief     Configures directories to search for source files.
 * @details   These paths are used to search source code when an import is encountered during compilation.
 * @param[in] options Target options.
 * @param[in] dir_count Number of directories.
 * @param[in] dirs Import directories.
 * @note      These paths are used when resolving imports if the callback passed to ::adv_options_set_importer
 *            did not return a source (if ::adv_options_set_importer was configured)
 * @sa        ::adv_options_get_import_dirs
 * @ingroup   functions
 */
ADV_API void
adv_options_set_import_dirs(adv_options_t options,
                            adv_uint32_t dir_count,
                            const adv_utf8_t* dirs);

/**
 * @brief      Get the current import callback.
 * @details    Returns a callback if one has been configured.
 * @param[in]  options Target options.
 * @param[out] data Returning a callback user data pointer (may be null).
 * @return     Returns a callback.
 * @sa         ::adv_options_set_importer
 * @ingroup    functions
 */
ADV_API adv_import_callback_t
adv_options_get_importer(adv_options_t options,
                         adv_data_t* data);

/**
 * @brief     Set import callback.
 * @details   Used to resolve code sources, such as when the compiler encounters imports.
 * @param[in] options Target options.
 * @param[in] callback Callback function.
 * @param[in] data Callback user data.
 * @parblock
 * @note      If set, the importer will be used to resolve sources as the highest
 *            priority (then the sources passed to ::adv_compiler_compile in the *sources*
 *            argument will be used, then the directories passed to ::adv_options_set_import_dirs will be used,
 *            and then the current working directory).
 * @endparblock
 * @parblock
 * @note      If *file* was not passed to ::adv_compiler_compile to compile from the file system,
 *            then the importer will also be used to obtain the main (primary) file named *\<input\>*.
 * @endparblock
 * @parblock
 * @note      A typical use of an importer is to read source code from memory.
 * @endparblock
 * @sa        ::adv_options_get_importer
 * @ingroup   functions
 */
ADV_API void
adv_options_set_importer(adv_options_t options,
                         adv_import_callback_t callback,
                         adv_data_t data);

/**
 * @brief      Get the current release import callback.
 * @details    Callback for releasing sources allocated via ::adv_options_set_importer.
 * @param[in]  options Target options.
 * @param[out] data Returning a callback user data pointer (may be null).
 * @return     Returns a callback.
 * @sa         ::adv_options_set_release_import
 * @ingroup    functions
 */
ADV_API adv_release_import_callback_t
adv_options_get_release_import(adv_options_t options,
                               adv_data_t* data);

/**
 * @brief     Set release import callback.
 * @details   If the callback set in ::adv_options_set_importer allocates data on the heap or creates
 *            any resources, they can be freed by the callback set here.
 * @param[in] options Target options.
 * @param[in] callback Callback function.
 * @param[in] data Callback user data.
 * @sa        ::adv_options_get_release_import
 * @ingroup   functions
 */
ADV_API void
adv_options_set_release_import(adv_options_t options,
                               adv_release_import_callback_t callback,
                               adv_data_t data);

/**
 * @brief      Get the current write callback.
 * @details    Returns a callback if one has been configured.
 * @param[in]  options Target options.
 * @param[out] data Returning a callback user data pointer (may be null).
 * @return     Returns a callback.
 * @sa         ::adv_options_set_writer
 * @ingroup    functions
 */
ADV_API adv_write_callback_t
adv_options_get_writer(adv_options_t options,
                       adv_data_t* data);

/**
 * @brief     Set write callback.
 * @details   Configures a callback to receive compiler output. If the callback is set, no output
 *            will be made to the file system (::adv_options_set_output_path will also not be used).
 * @param[in] options Target options.
 * @param[in] callback Callback function.
 * @param[in] data Callback user data.
 * @note      Typical uses of a writer are writing to memory or outputting to the console and the like.
 * @sa        ::adv_options_get_writer
 * @ingroup   functions
 */
ADV_API void
adv_options_set_writer(adv_options_t options,
                       adv_write_callback_t callback,
                       adv_data_t data);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ADV_OPTIONS_H */
