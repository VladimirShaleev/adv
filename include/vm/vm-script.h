/**
 * @file      vm-script.h
 * @brief     Script file format and byte-order utilities for the virtual machine.
 * @details   Defines the binary script header and section table, along with
 *            validation and byte-swapping functions. Multibyte values in the
 *            file, including section values and bytecode operands, are stored
 *            in little-endian byte order.
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   files
 * @copyright MIT License
 */
#ifndef VM_SCRIPT_H
#define VM_SCRIPT_H

#include "vm-types.h"

#ifdef __cplusplus
extern "C" {
#endif

VM_PACKED_BEGIN

/**
 * @brief   Header of a compiled VM script.
 * @details The magic field is stored as four raw bytes. The version and
 *          checksum fields are always stored in little-endian byte order.
 *          Big-endian hosts must use the byte-swapping helpers when reading
 *          or writing multibyte values.
 * @ingroup structs
 */
typedef struct
{
    vm_char_t   magic[4]; /**< Four-byte script format identifier. */
    vm_uint32_t version; /**< Script format version, stored little-endian. */
    vm_uint32_t checksum; /**< Adler-32 checksum, stored little-endian. */
} VM_PACKED_ATTR vm_script_header_t;

/**
 * @brief   Offsets and sizes of the script data and bytecode sections.
 * @details The data block contains string entries. Each entry starts with a
 *          little-endian vm_uint16_t string length, excluding the terminating
 *          null character, followed by the string and its null terminator.
 *          The text block contains bytecode made up of vm_opcode_t opcodes
 *          and their operands. All multibyte values, including these fields
 *          and operands, are stored in little-endian byte order. Big-endian
 *          hosts must convert multibyte values with the byte-swapping helpers.
 * @ingroup structs
 */
typedef struct
{
    vm_uint32_t export_offset; /**< Byte offfset of the export symbols block. */
    vm_uint32_t export_size; /**< Size in bytes of the export symbols block */
    vm_uint32_t data_offset; /**< Byte offset of the string data block. */
    vm_uint32_t data_size; /**< Size in bytes of the string data block. */
    vm_uint32_t text_offset; /**< Byte offset of the bytecode block. */
    vm_uint32_t text_size; /**< Size in bytes of the bytecode block. */
} VM_PACKED_ATTR vm_script_table_t;

/**
 * @brief   Export table metadata for the script symbol block.
 * @details Stores the number and byte size of exported variables and
 *          functions. Values are kept in little-endian byte order and are
 *          converted with vm_swap_script_export when read on big-endian hosts.
 * @ingroup structs
 */
typedef struct
{
    vm_uint32_t module_name;
    vm_uint16_t root_frame_slots;
    vm_uint16_t var_count; /**< Number of exported variables. */
    vm_uint16_t var_size; /**< Size in bytes of the variable export block. */
    vm_uint16_t func_count; /**< Number of exported functions. */
    vm_uint16_t func_size; /**< Size in bytes of the function export block. */
    vm_uint16_t native_count; /**< Number of exported native functions. */
    vm_uint16_t native_size; /**< Size in bytes of the native function export block. */
} VM_PACKED_ATTR vm_script_export_t;

/**
 * @brief   Exported variable metadata.
 * @details Describes a single global variable symbol stored in the script
 *          export block. The name field indexes a string in the script data
 *          section, while the type field identifies the variable kind.
 * @ingroup structs
 */
typedef struct
{
    vm_uint32_t name; /**< Offset or index of the variable name in the data block. */
    vm_uint8_t  type; /**< Type code of the exported variable. */
} VM_PACKED_ATTR vm_script_export_var_t;

typedef struct
{
    vm_uint32_t name;
    vm_uint8_t  type;
} VM_PACKED_ATTR vm_script_export_param_t;

/**
 * @brief   Exported function metadata.
 * @details Describes a single function symbol stored in the script export
 *          block. The name field indexes a string in the script data section,
 *          while the text offset points to the function bytecode in the text
 *          block.
 * @ingroup structs
 */
typedef struct
{
    vm_uint32_t name; /**< Offset or index of the function name in the data block. */
    vm_uint32_t text_offset; /**< Byte offset of the function body in the text block. */
    vm_uint16_t frame_slot_count; /**< Number of slots reserved for the function frame. */
    vm_uint8_t  return_type; /**< Type code returned by the function. */
    vm_uint8_t  param_count; /**< Number of parameters expected by the function. */
    /* vm_script_export_param_t params[param_count]; */
} VM_PACKED_ATTR vm_script_export_func_t;

typedef struct
{
    vm_uint32_t name; /**< Offset or index of the native function name in the data block. */
    vm_uint8_t  return_type; /**< Type code returned by the native function. */
    vm_uint8_t  param_count; /**< Number of parameters expected by the native function. */
    /* vm_script_export_param_t params[param_count]; */
} VM_PACKED_ATTR vm_script_export_native_t;

VM_PACKED_END

/**
 * @name Functions for Script Handling.
 * @brief Functions for handling compiled VM scripts.
 * @{
 */

/**
 * @note Serialized script data is always stored in little-endian byte order
 *       in the binary format. The `vm_read_*` helpers read the raw payload,
 *       convert it to the native byte order of the current host, and return
 *       values in host-endian representation. In other words, callers always
 *       receive `vm_uint16_t`, `vm_uint32_t`, `vm_sint32_t`, and floating-point
 *       values as native CPU-order values, while the on-disk format remains
 *       little-endian for all `int16`, `int32`, and larger fields.
 * @ingroup functions
 */

/**
 * @brief     Calculates an Adler-32 checksum.
 * @param[in] data Bytes to checksum.
 * @param[in] size Number of bytes in @p data.
 * @return    Adler-32 checksum value.
 * @ingroup   functions
 */
VM_API vm_uint32_t
vm_adler_32(const vm_char_t* buffer, 
            vm_uint32_t size);

/**
 * @brief     Validates the checksum in a script.
 * @param[in] buffer Script bytes.
 * @param[in] size Number of bytes in @p data.
 * @return    *TRUE* if the checksum is valid; otherwise *FALSE*.
 * @ingroup   functions
 */
VM_API vm_bool_t
vm_validate_checksum(const vm_char_t* buffer, 
                     vm_uint32_t size);

/**
 * @brief     Validates the version in a script.
 * @param[in] buffer Script bytes.
 * @param[in] size Number of bytes in @p data.
 * @return    *TRUE* if the version is supported; otherwise *FALSE*.
 * @ingroup   functions
 */
VM_API vm_bool_t
vm_validate_version(const vm_char_t* buffer, 
                    vm_uint32_t size);

/**
 * @brief         Reads a 16-bit value from a script buffer.
 * @details       The serialized script payload is little-endian. This helper
 *                reads the raw 16-bit value, converts it to the host byte order,
 *                and returns the decoded integer in the native representation of
 *                the current platform.
 * @param[in]     buffer Script data buffer.
 * @param[in]     size Total number of bytes available in @p buffer.
 * @param[in,out] offset Current read offset. Updated to the next unread byte.
 * @return        Unsigned 16-bit value in host byte order.
 * @ingroup       functions
 */
VM_API vm_uint16_t
vm_read_uint16(const vm_char_t* buffer,
               vm_uint32_t size,
               vm_uint32_t* offset);

/**
 * @brief         Reads a 32-bit value from a script buffer.
 * @details       The serialized VM data is little-endian. This helper converts
 *                the value from little-endian storage to the host byte order
 *                before returning it to the caller.
 * @param[in]     buffer Script data buffer.
 * @param[in]     size Total number of bytes available in @p buffer.
 * @param[in,out] offset Current read offset. Updated to the next unread byte.
 * @return        Unsigned 32-bit value in host byte order.
 * @ingroup       functions
 */
VM_API vm_uint32_t
vm_read_uint32(const vm_char_t* buffer,
               vm_uint32_t size,
               vm_uint32_t* offset);

/**
 * @brief         Reads a signed 32-bit value from a script buffer.
 * @details       The raw script payload is stored in little-endian order. This
 *                helper converts the serialized value to the host byte order
 *                before returning the signed integer.
 * @param[in]     buffer Script data buffer.
 * @param[in]     size Total number of bytes available in @p buffer.
 * @param[in,out] offset Current read offset. Updated to the next unread byte.
 * @return        Signed 32-bit value in host byte order.
 * @ingroup       functions
 */
VM_API vm_sint32_t
vm_read_int32(const vm_char_t* buffer,
              vm_uint32_t size,
              vm_uint32_t* offset);

/**
 * @brief         Reads a 32-bit IEEE-754 float from a script buffer.
 * @details       Script data is serialized in little-endian format. This helper
 *                converts the raw little-endian representation to the host byte
 *                order and returns the native floating-point value.
 * @param[in]     buffer Script data buffer.
 * @param[in]     size Total number of bytes available in @p buffer.
 * @param[in,out] offset Current read offset. Updated to the next unread byte.
 * @return        Floating-point value in host byte order.
 * @ingroup       functions
 */
VM_API vm_float32_t
vm_read_float(const vm_char_t* buffer,
              vm_uint32_t size,
              vm_uint32_t* offset);

/**
 * @brief      Reads the script header from a buffer.
 * @details    Decodes the format header and stores the result in @p header.
 * @param[in]  buffer Script data buffer.
 * @param[in]  size Total number of bytes available in @p buffer.
 * @param[out] header Destination header structure.
 * @return     Pointer to @p header on success, or `NULL` on failure.
 * @ingroup    functions
 */
VM_API vm_script_header_t*
vm_read_script_header(const vm_char_t* buffer,
                      vm_uint32_t size,
                      vm_script_header_t* header);

/**
 * @brief      Reads the script section table from a buffer.
 * @details    Decodes the section table and stores it in @p table.
 * @param[in]  buffer Script data buffer.
 * @param[in]  size Total number of bytes available in @p buffer.
 * @param[out] table Destination section table structure.
 * @return     Pointer to @p table on success, or `NULL` on failure.
 * @ingroup    functions
 */
VM_API vm_script_table_t*
vm_read_script_table(const vm_char_t* buffer,
                     vm_uint32_t size,
                     vm_script_table_t* table);

/**
 * @brief      Reads the script export table from a buffer.
 * @details    Decodes the export metadata block stored in the script image.
 * @param[in]  buffer Script data buffer.
 * @param[in]  size Total number of bytes available in @p buffer.
 * @param[out] export_table Destination export table structure.
 * @return     Pointer to @p export_table on success, or `NULL` on failure.
 * @ingroup    functions
 */
VM_API vm_script_export_t*
vm_read_script_export(const vm_char_t* buffer,
                      vm_uint32_t size,
                      vm_script_export_t* export_table);

/**
 * @brief      Reads one exported variable descriptor.
 * @details    Decodes the variable entry at @p var_index from the export table.
 * @param[in]  buffer Script data buffer.
 * @param[in]  size Total number of bytes available in @p buffer.
 * @param[in]  var_index  Variable index to read.
 * @param[out] export_var Destination variable descriptor.
 * @return     Pointer to @p export_var on success, or `NULL` on failure.
 * @ingroup    functions
 */
VM_API vm_script_export_var_t*
vm_read_script_export_var(const vm_char_t* buffer,
                          vm_uint32_t size,
                          vm_uint32_t var_index,
                          vm_script_export_var_t* export_var);

/**
 * @brief      Reads one exported function descriptor.
 * @details    Decodes the function entry at @p func_index from the export table.
 * @param[in]  buffer Script data buffer.
 * @param[in]  size Total number of bytes available in @p buffer.
 * @param[in]  func_index Function index to read.
 * @param[out] export_func Destination function descriptor.
 * @return     Pointer to @p export_func on success, or `NULL` on failure.
 * @ingroup    functions
 */
VM_API vm_script_export_func_t*
vm_read_script_export_func(const vm_char_t* buffer,
                           vm_uint32_t size,
                           vm_uint32_t func_index,
                           vm_script_export_func_t* export_func);

/**
 * @brief      Reads a function parameter descriptor.
 * @details    Decodes the parameter metadata for a function entry.
 * @param[in]  buffer Script data buffer.
 * @param[in]  size Total number of bytes available in @p buffer.
 * @param[in]  func_index Function index whose parameter is being read.
 * @param[in]  param_index Parameter index to read.
 * @param[out] param Destination parameter descriptor.
 * @return     Pointer to @p param on success, or `NULL` on failure.
 * @ingroup    functions
 */
VM_API vm_script_export_param_t*
vm_read_script_export_func_param(const vm_char_t* buffer,
                                 vm_uint32_t size,
                                 vm_uint32_t func_index,
                                 vm_uint32_t param_index,
                                 vm_script_export_param_t* param);

/**
 * @brief      Reads one exported native function descriptor.
 * @details    Decodes the native entry at @p native_index from the export table.
 * @param[in]  buffer Script data buffer.
 * @param[in]  size Total number of bytes available in @p buffer.
 * @param[in]  native_index Native function index to read.
 * @param[out] export_native Destination native descriptor.
 * @return     Pointer to @p export_native on success, or `NULL` on failure.
 * @ingroup    functions
 */
VM_API vm_script_export_native_t*
vm_read_script_export_native(const vm_char_t* buffer,
                           vm_uint32_t size,
                           vm_uint32_t native_index,
                           vm_script_export_native_t* export_native);

/**
 * @brief      Reads a native function parameter descriptor.
 * @details    Decodes the parameter metadata for a native entry.
 * @param[in]  buffer Script data buffer.
 * @param[in]  size Total number of bytes available in @p buffer.
 * @param[in]  native_index Native function index whose parameter is being read.
 * @param[in]  param_index  Parameter index to read.
 * @param[out] param Destination parameter descriptor.
 * @return     Pointer to @p param on success, or `NULL` on failure.
 * @ingroup    functions
 */
VM_API vm_script_export_param_t*
vm_read_script_export_native_param(const vm_char_t* buffer,
                                   vm_uint32_t size,
                                   vm_uint32_t native_index,
                                   vm_uint32_t param_index,
                                   vm_script_export_param_t* param);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* VM_SCRIPT_H */
