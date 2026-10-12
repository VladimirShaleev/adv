/**
 * @file      adv.h
 * @brief     ADV
 * @details   ADV - Adventure compiler for generating
 *            bytecode for a Virtual Machine.
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
#ifndef ADV_H
#define ADV_H

#include "adv-options.h"
#include "adv-version.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name Functions of Compiler.
 * @brief Functions for opaque type ::adv_compiler_t.
 * @{
 */

/**
 * @brief      Creates new compiler instance.
 * @details    Creates an object for ADV compilation.
 * @param[out] compiler New compiler instance.
 * @return     New compiler instance
 * @ingroup    functions
 */
ADV_API adv_result_t
adv_compiler_create(adv_compiler_t* compiler);

/**
 * @brief     Increments reference count.
 * @details   Manages compiler instance lifetime.
 * @param[in] compiler Target compiler instance.
 * @return    Reference to same compiler.
 * @sa        ::adv_compiler_destroy
 * @ingroup   functions
 */
ADV_API adv_compiler_t
adv_compiler_reference(adv_compiler_t compiler);

/**
 * @brief     Releases compiler instance.
 * @details   Destroys when reference count reaches zero.
 * @param[in] compiler Compiler to destroy.
 * @sa        ::adv_compiler_reference
 * @ingroup   functions
 */
ADV_API void
adv_compiler_destroy(adv_compiler_t compiler);

/**
 * @brief      Compiles a source file or in-memory source.
 * @details    If @p file is not `NULL` and is not empty, it identifies the
 *             primary source file. Otherwise, the compiler takes the primary
 *             source from @p sources. If neither is provided, there is no
 *             source to compile.
 *
 *             Imports are resolved first through the importer configured with
 *             ::adv_options_set_importer, then through the entries in
 *             @p sources. If neither supplies the module, the compiler searches
 *             the file system, including the importing file's directory, the
 *             configured import directories, and the current working directory.
 *
 *             The returned ::adv_result_t reports whether compilation succeeded
 *             regardless of whether @p result is `NULL`. When @p result is not
 *             `NULL`, it receives a compilation result object containing
 *             diagnostic messages and, when available, the syntax tree and
 *             symbol table.
 * @param[in]  compiler Compiler instance.
 * @param[in]  file Path to the primary `.adv` file, or `NULL`/empty to compile
 *             the primary source from @p sources.
 * @param[in]  source_count Number of entries in @p sources.
 * @param[in]  sources In-memory source files. May be `NULL` when
 *             @p source_count is zero.
 * @param[in]  options Compilation options, or `NULL` to use default options.
 * @param[out] result Receives the compilation result object, or may be `NULL`
 *             if detailed diagnostics and compilation data are not needed.
 * @return     ::ADV_RESULT_SUCCESS on success, or an error result describing
 *             why compilation did not succeed.
 * @note       To compile entirely from memory, pass an empty @p file and provide
 *             the primary file in @p sources.
 * @ingroup    functions
 */
ADV_API adv_result_t
adv_compiler_compile(adv_compiler_t compiler,
                     adv_utf8_t file,
                     adv_uint32_t source_count,
                     const adv_source_t* sources,
                     adv_options_t options,
                     adv_compilation_result_t* result);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ADV_H */
