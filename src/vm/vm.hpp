#ifndef VM_HPP
#define VM_HPP

#ifndef VM_STATIC_BUILD
#ifdef _MSC_VER
#define VM_API __declspec(dllexport)
#elif __GNUC__ >= 4
#define VM_API __attribute__((visibility("default")))
#else
#define VM_API
#endif
#endif

#include "vm/vm.h"

#endif // VM_HPP
