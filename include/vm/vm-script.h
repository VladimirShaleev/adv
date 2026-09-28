#ifndef VM_SCRIPT_H
#define VM_SCRIPT_H

#include "vm-platform.h"

#ifdef __cplusplus
extern "C" {
#endif

VM_PACKED_BEGIN

typedef struct
{
    vm_char_t   magic[4];
    vm_uint32_t version;
    vm_uint32_t checksum;
} VM_PACKED_ATTR vm_script_header_t;

typedef struct
{
    vm_uint32_t data_offset;
    vm_uint32_t data_size;
    vm_uint32_t text_offset;
    vm_uint32_t text_size;
} VM_PACKED_ATTR vm_script_table_t;

VM_PACKED_END

#ifdef __cplusplus
}
#endif

#endif /* VM_SCRIPT_H */
