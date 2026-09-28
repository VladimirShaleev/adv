#ifndef VM_VM_HPP
#define VM_VM_HPP

#ifndef VM_STATIC_BUILD
#ifdef _MSC_VER
#define VM_API __declspec(dllexport)
#elif __GNUC__ >= 4
#define VM_API __attribute__((visibility("default")))
#else
#define VM_API
#endif
#endif

#include <span>
#include <vector>

#include "vm/vm.h"

#include <utils/utils.hpp>

namespace utils {
template <>
struct Result<vm_result_t> {
    static constexpr vm_result_t Success = VM_RESULT_SUCCESS;
    static constexpr vm_result_t Error = VM_RESULT_ERROR_UNKNOWN;
    static constexpr vm_result_t OutOfMemory = VM_RESULT_ERROR_OUT_OF_MEMORY;
};
} // namespace utils

#endif // VM_VM_HPP
