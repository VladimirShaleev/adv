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
    vm_uint16_t var_count; /**< Number of exported variables. */
    vm_uint16_t var_size; /**< Size in bytes of the variable export block. */
    vm_uint16_t func_count; /**< Number of exported functions. */
    vm_uint16_t func_size; /**< Size in bytes of the function export block. */
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
    /* vm_script_export_var_t params[param_count]; */
} VM_PACKED_ATTR vm_script_export_func_t;

VM_PACKED_END

/**
 * @name Functions for Script Handling.
 * @brief Functions for handling compiled VM scripts.
 * @{
 */

/**
 * @brief      Calculates an Adler-32 checksum.
 * @param[in]  data Bytes to checksum.
 * @param[in]  size Number of bytes in @p data.
 * @return     Adler-32 checksum value.
 * @ingroup    functions
 */
VM_API vm_uint32_t
vm_adler_32(const vm_char_t* data, 
            vm_uint32_t size);

/**
 * @brief      Validates the checksum in a script.
 * @param[in]  data Script bytes.
 * @param[in]  size Number of bytes in @p data.
 * @return     *TRUE* if the checksum is valid; otherwise *FALSE*.
 * @ingroup    functions
 */
VM_API vm_bool_t
vm_validate_checksum(const vm_char_t* data, 
                     vm_uint32_t size);

/**
 * @brief      Validates the version in a script.
 * @param[in]  data Script bytes.
 * @param[in]  size Number of bytes in @p data.
 * @return     *TRUE* if the version is supported; otherwise *FALSE*.
 * @ingroup    functions
 */
VM_API vm_bool_t
vm_validate_version(const vm_char_t* data, 
                    vm_uint32_t size);

/**
 * @brief   Converts a 16-bit value between host and little-endian byte order.
 * @details Does nothing on little-endian hosts and swaps the bytes on
 *          big-endian hosts.
 * @param   value Value to convert.
 * @return  Converted value.
 * @ingroup functions
 */
VM_API vm_uint16_t
vm_swap_uint16(vm_uint16_t value);

/**
 * @brief   Converts a 32-bit value between host and little-endian byte order.
 * @details Does nothing on little-endian hosts and swaps the bytes on
 *          big-endian hosts.
 * @param   value Value to convert.
 * @return  Converted value.
 * @ingroup functions
 */
VM_API vm_uint32_t
vm_swap_uint32(vm_uint32_t value);

/**
 * @brief   Converts a 32-bit float between host and little-endian byte order.
 * @details Does nothing on little-endian hosts and swaps the bytes of its
 *          representation on big-endian hosts.
 * @param   value Value to convert.
 * @return  Converted value.
 * @ingroup functions
 */
VM_API vm_float32_t
vm_swap_float32(vm_float32_t value);

/**
 * @brief      Converts a script header between host and little-endian order.
 * @details    Does nothing on little-endian hosts and swaps the multibyte
 *             fields on big-endian hosts.
 * @param[in]  header Header to convert.
 * @return     Converted header, or a zero-initialized header if NULL.
 * @ingroup    functions
 */
VM_API vm_script_header_t
vm_swap_script_header(const vm_script_header_t* header);

/**
 * @brief      Converts a script table between host and little-endian order.
 * @details    Does nothing on little-endian hosts and swaps all fields on
 *             big-endian hosts.
 * @param[in]  table Table to convert.
 * @return     Converted table, or a zero-initialized table if NULL.
 * @ingroup    functions
 */
VM_API vm_script_table_t
vm_swap_script_table(const vm_script_table_t* table);

/**
 * @brief      Converts an export table between host and little-endian order.
 * @details    Does nothing on little-endian hosts and swaps the multibyte
 *             fields in the export metadata on big-endian hosts.
 * @param[in]  export_table Export table to convert.
 * @return     Converted export table, or a zero-initialized table if NULL.
 * @ingroup    functions
 */
VM_API vm_script_export_t
vm_swap_script_export(const vm_script_export_t* export_table);

/**
 * @brief      Converts an exported variable descriptor between host and
 *             little-endian order.
 * @details    Does nothing on little-endian hosts and swaps the multibyte
 *             fields on big-endian hosts.
 * @param[in]  export_var Variable descriptor to convert.
 * @return     Converted variable descriptor, or a zero-initialized value if
 *             NULL.
 * @ingroup    functions
 */
VM_API vm_script_export_var_t
vm_swap_script_export_var(const vm_script_export_var_t* export_var);

/**
 * @brief      Converts an exported function descriptor between host and
 *             little-endian order.
 * @details    Does nothing on little-endian hosts and swaps the multibyte
 *             fields on big-endian hosts.
 * @param[in]  export_func Function descriptor to convert.
 * @return     Converted function descriptor, or a zero-initialized value if
 *             NULL.
 * @ingroup    functions
 */
VM_API vm_script_export_func_t
vm_swap_script_export_func(const vm_script_export_func_t* export_func);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* VM_SCRIPT_H */
