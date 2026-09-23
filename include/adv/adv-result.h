/**
 * @file      adv-result.h
 * @brief     Warning/Error codes.
 * @details   Here are the warning and error codes that may occur during compilation.
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   files
 * @copyright MIT License
 */
#ifndef ADV_RESULT_H
#define ADV_RESULT_H

#include "adv-ast.h"
#include "adv-types.h"

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
    ADV_RESULT_SUCCESS                = 0, /**< Indicates success (this is not an error). */
    ADV_RESULT_COMPILATION_FAILED     = 1, /**< Compilation failed. */
    ADV_RESULT_ERROR_UNKNOWN          = 2, /**< Unknown error. */
    ADV_RESULT_ERROR_OUT_OF_MEMORY    = 3, /**< Out of memory. */
    ADV_RESULT_ERROR_INVALID_ARG      = 4, /**< Invalid argument. */
    ADV_RESULT_ERROR_FILE_CREATE      = 5, /**< Failed to create file. */
    ADV_RESULT_ERROR_SOURCE_NOT_FOUND = 6, /**< Source not found. */
    ADV_RESULT_COUNT_ENUMS            = 7, /**< Count of constants in the enumeration. */
    ADV_RESULT_MAX_ENUM               = 0x7FFFFFFF /**< Max value of enum (not used) */
} adv_result_t;

/**
 * @brief   Compilation statuses.
 * @details This enumeration contains warnings and errors that may occur during compilation.
 * @ingroup enums
 */
typedef enum
{
    ADV_STATUS_N1001       = 1001, /**< Symbol name shadowing in an outer scope. */
    ADV_STATUS_W2001       = 2001, /**< Value out of range. */
    ADV_STATUS_W2002       = 2002, /**< Implicit conversion in return statement may lose precision */
    ADV_STATUS_W2003       = 2003, /**< Implicit conversion in binary operation may lose precision. */
    ADV_STATUS_W2004       = 2004, /**< Implicit conversion for argument in call may lose precision. */
    ADV_STATUS_W2005       = 2005, /**< Implicit conversion in assignment may lose precision. */
    ADV_STATUS_W2006       = 2006, /**< Implicit conversion in variable initialization may lose precision. */
    ADV_STATUS_E3001       = 3001, /**< Syntax error. */
    ADV_STATUS_E3002       = 3002, /**< Unknown error. */
    ADV_STATUS_E3003       = 3003, /**< String closing character not found. */
    ADV_STATUS_E3004       = 3004, /**< Unterminated block comment. */
    ADV_STATUS_E3005       = 3005, /**< Invalid escape character. */
    ADV_STATUS_E3006       = 3006, /**< Failed to infer the argument type. */
    ADV_STATUS_E3007       = 3007, /**< Variable declaration requires a type annotation or an initializer. */
    ADV_STATUS_E3008       = 3008, /**< Symbol is already declared in this scope. */
    ADV_STATUS_E3009       = 3009, /**< Symbol not found. */
    ADV_STATUS_E3010       = 3010, /**< Unknown target type in explicit cast expression. */
    ADV_STATUS_E3011       = 3011, /**< Unknown return type in declaration of function. */
    ADV_STATUS_E3012       = 3012, /**< Cannot explicitly convert expression. */
    ADV_STATUS_E3013       = 3013, /**< Function with no return type cannot return a value. */
    ADV_STATUS_E3014       = 3014, /**< Non-void function must return a value. */
    ADV_STATUS_E3015       = 3015, /**< Cannot convert return expression of type to return type in function. */
    ADV_STATUS_E3016       = 3016, /**< Parameter must have an explicit type annotation. */
    ADV_STATUS_E3017       = 3017, /**< Unknown type specified for parameter. */
    ADV_STATUS_E3018       = 3018, /**< Variable or symbol is used before its type is resolved. */
    ADV_STATUS_E3019       = 3019, /**< Invalid operands to binary expression, incompatible types. */
    ADV_STATUS_E3020       = 3020, /**< Call to undeclared function. */
    ADV_STATUS_E3021       = 3021, /**< Symbol is not a function and cannot be called. */
    ADV_STATUS_E3022       = 3022, /**< The function does not accept the number of arguments passed to it. */
    ADV_STATUS_E3023       = 3023, /**< Cannot convert argument to parameter type in call. */
    ADV_STATUS_E3024       = 3024, /**< Cannot assign value to target of type. */
    ADV_STATUS_E3025       = 3025, /**< Variable declaration must have either an explicit type annotation or an initializer. */
    ADV_STATUS_E3026       = 3026, /**< Unknown type in declaration of variable. */
    ADV_STATUS_E3027       = 3027, /**< Cannot initialize variable with expression. */
    ADV_STATUS_COUNT_ENUMS = 34, /**< Count of constants in the enumeration. */
    ADV_STATUS_MAX_ENUM    = 0x7FFFFFFF /**< Max value of enum (not used) */
} adv_status_t;

/**
 * @brief   Compilation message.
 * @details Detailed description of warning or compilation error.
 * @ingroup structs
 */
typedef struct
{
    adv_status_t status; /**< Compilation status. */
    adv_bool_t   is_error; /**< The message indicates an error. */
    adv_utf8_t   message; /**< Detailed text description. */
    adv_utf8_t   filename; /**< File in which warning or error was detected. */
    adv_uint32_t line; /**< The line number where the warning or error was detected. */
    adv_uint32_t column; /**< The column in which the warning or error was detected. */
} adv_message_t;

/**
 * @brief     Converts error code to descriptive string.
 * @details   Provides a text description for the result code.
 * @param[in] result Result code.
 * @return    Corresponding text description of the result code.
 * @ingroup   functions
 */
ADV_API adv_utf8_t
adv_result_to_string(adv_result_t result);

/**
 * @name Functions of Compilation Result.
 * @brief Functions for opaque type ::adv_compilation_result_t.
 * @{
 */

/**
 * @brief     Increments reference count.
 * @details   Manages compilation result instance lifetime.
 * @param[in] compilation_result Target compilation result instance.
 * @return    Reference to same compilation result.
 * @sa        ::adv_compilation_result_destroy
 * @ingroup   functions
 */
ADV_API adv_compilation_result_t
adv_compilation_result_reference(adv_compilation_result_t compilation_result);

/**
 * @brief     Releases compilation result instance.
 * @details   Destroys when reference count reaches zero.
 * @param[in] compilation_result Compilation result to destroy.
 * @sa        ::adv_compilation_result_reference
 * @ingroup   functions
 */
ADV_API void
adv_compilation_result_destroy(adv_compilation_result_t compilation_result);

/**
 * @brief     Checking if there were notes.
 * @details   Check if there were any notes during compilation.
 * @param[in] compilation_result Target compilation result instance.
 * @return    *TRUE*, if there are notes.
 * @ingroup   functions
 */
ADV_API adv_bool_t
adv_compilation_result_has_notes(adv_compilation_result_t compilation_result);

/**
 * @brief     Checking if there were warnings.
 * @details   Check if there were any warnings during compilation.
 * @param[in] compilation_result Target compilation result instance.
 * @return    *TRUE*, if there are warnings.
 * @ingroup   functions
 */
ADV_API adv_bool_t
adv_compilation_result_has_warnings(adv_compilation_result_t compilation_result);

/**
 * @brief     Checking if there were errors.
 * @details   Check if there were any errors during compilation.
 * @param[in] compilation_result Target compilation result instance.
 * @return    *TRUE*, if there are errors.
 * @ingroup   functions
 */
ADV_API adv_bool_t
adv_compilation_result_has_errors(adv_compilation_result_t compilation_result);

/**
 * @brief         Returns messages with warnings and errors.
 * @details       Returns messages with warnings and errors that occurred during compilation.
 * @param[in]     compilation_result Target compilation result instance.
 * @param[in,out] message_count Number of messages.
 * @param[out]    messages Message array.
 * @ingroup       functions
 */
ADV_API void
adv_compilation_result_get_messages(adv_compilation_result_t compilation_result,
                                    adv_uint32_t* message_count,
                                    adv_message_t* messages);

/**
 * @brief     Get the root node of the syntax tree.
 * @details   Returns the root node resulting from compile the source code via ::adv_compiler_compile.
 * @param[in] compilation_result Target compilation result instance.
 * @return    Root node of the syntactic tree.
 * @ingroup   functions
 */
ADV_API const adv_node_t*
adv_compilation_result_get_root_node(adv_compilation_result_t compilation_result);

/**
 * @brief     Get C string by string id.
 * @details   Returns the C string by ::vm_string_t.
 * @param[in] compilation_result Target compilation result instance.
 * @param[in] string String id.
 * @return    C string.
 * @ingroup   functions
 */
ADV_API adv_utf8_t
adv_compilation_result_get_string(adv_compilation_result_t compilation_result,
                                  vm_string_h string);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ADV_RESULT_H */
