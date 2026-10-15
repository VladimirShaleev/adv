/**
 * @file      vm-platform.h
 * @brief     Platform-specific definitions and utilities.
 * @details   This header provides cross-platform macros, type definitions, and utility
 *            macros for the Adv library. It handles:
 *            - Platform detection (Windows, macOS, iOS, Android, Linux, Web)
 *            - Symbol visibility control (DLL import/export on Windows)
 *            - C/C++ interoperability
 *            - Type definitions for consistent data sizes across platforms
 *            - Bit flag operations for enumerations (C++ only).
 *            
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   vm_files
 * @copyright MIT License
 */
#ifndef VM_PLATFORM_H
#define VM_PLATFORM_H

/**
 * @def     VM_API
 * @brief   Controls symbol visibility for shared library builds.
 * @details This macro is used to control symbol visibility when building or using the library.
 *          On Windows (**MSVC**) with dynamic linking (non-static build), it expands to `__declspec(dllimport)`.
 *          In all other cases (static builds or non-Windows platforms), it expands to nothing.
 *          This allows proper importing of symbols from DLLs on Windows platforms.
 * @note    Define `VM_STATIC_BUILD` for static library configuration.
 * @ingroup vm_macros
 */

#ifndef VM_API
# if defined(_MSC_VER) && !defined(VM_STATIC_BUILD)
#  define VM_API __declspec(dllimport)
# else
#  define VM_API
# endif
#endif

/**
 * @def     VM_PACKED_BEGIN
 * @brief   Opens a packed-structure region for compilers that support it.
 * @details Marks the following declarations as tightly packed to minimize struct
 *          padding on supported compilers. This is used for binary file headers
 *          and table layouts that must match a fixed on-disk layout.
 * @note    This macro is a compatibility helper and is intentionally left empty
 *          on compilers that do not require explicit packing directives.
 * @ingroup vm_macros
 */

/**
 * @def     VM_PACKED_END
 * @brief   Closes a packed-structure region.
 * @details Restores the compiler's previous packing alignment after a
 *          VM_PACKED_BEGIN block.
 * @ingroup vm_macros
 */

/**
 * @def     VM_PACKED_ATTR
 * @brief   Attribute applied to packed structs.
 * @details GCC/Clang use `__attribute__((packed))`, while MSVC and other
 *          compilers rely on pragma-based packing directives.
 * @ingroup vm_macros
 */

#if defined(_MSC_VER)
# define VM_PACKED_BEGIN _Pragma("pack(push, 1)")
# define VM_PACKED_END   _Pragma("pack(pop)")
# define VM_PACKED_ATTR
#elif defined(__GNUC__) || defined(__clang__)
# define VM_PACKED_BEGIN
# define VM_PACKED_END
# define VM_PACKED_ATTR __attribute__((packed))
#else
# define VM_PACKED_BEGIN
# define VM_PACKED_END
# define VM_PACKED_ATTR
#endif

#if defined(_WIN32) && !defined(VM_PLATFORM_WINDOWS)
# define VM_PLATFORM_WINDOWS
#elif defined(__APPLE__)
# include <TargetConditionals.h>
# include <unistd.h>
# if TARGET_OS_IPHONE && !defined(VM_PLATFORM_IOS)
#  define VM_PLATFORM_IOS
# elif TARGET_IPHONE_SIMULATOR && !defined(VM_PLATFORM_IOS)
#  define VM_PLATFORM_IOS
# elif TARGET_OS_MAC && !defined(VM_PLATFORM_MAC_OS)
#  define VM_PLATFORM_MAC_OS
# else
#  error unsupported Apple platform
# endif
#elif defined(__ANDROID__) && !defined(VM_PLATFORM_ANDROID)
# define VM_PLATFORM_ANDROID
#elif defined(__EMSCRIPTEN__) && !defined(VM_PLATFORM_WEB)
# define VM_PLATFORM_WEB
#elif defined(__linux__) && !defined(VM_PLATFORM_LINUX)
# define VM_PLATFORM_LINUX
#else
# error unsupported platform
#endif

/**
 * @addtogroup vm_types
 * @{
 */

/**
 * @name  Platform-independent type definitions.
 * @brief Fixed-size types guaranteed to work across all supported platforms.
 * @{
 */
#include <stdint.h>
typedef char        vm_char_t;    /**< symbol type. */
typedef int32_t     vm_bool_t;    /**< boolean type. */
typedef int8_t      vm_sint8_t;   /**< 8 bit signed integer. */
typedef uint8_t     vm_uint8_t;   /**< 8 bit unsigned integer. */
typedef int16_t     vm_sint16_t;  /**< 16 bit signed integer. */
typedef uint16_t    vm_uint16_t;  /**< 16 bit unsigned integer. */
typedef int32_t     vm_sint32_t;  /**< 32 bit signed integer. */
typedef uint32_t    vm_uint32_t;  /**< 32 bit unsigned integer. */
typedef int64_t     vm_sint64_t;  /**< 64 bit signed integer. */
typedef uint64_t    vm_uint64_t;  /**< 64 bit unsigned integer. */
typedef float       vm_float32_t; /**< 32 bit float point. */
typedef double      vm_float64_t; /**< 64 bit float point. */
typedef const char* vm_utf8_t;    /**< utf8 string. */
typedef void*       vm_data_t;    /**< pointer to data. */
/** @} */

/** @} */

/**
 * @def       VM_TYPE
 * @brief     Declares an opaque handle type.
 * @details   Creates a typedef for a pointer to an incomplete struct type,
 *            providing type safety while hiding implementation details.
 * @param[in] vm_name Base name for the type (suffix `_t` will be added).
 * @ingroup   vm_macros
 */
#define VM_TYPE(vm_name) \
typedef struct _##vm_name* vm_name##_t;

/**
 * @def       VM_HANDLE
 * @brief     Declares an index-based handle type.
 * @details   Creates a struct containing an index value, typically used for
 *            resource handles in API designs that avoid direct pointers.
 * @param[in] vm_name Base name for the handle type (suffix `_h` will be added).
 * @ingroup   vm_macros
 */
#define VM_HANDLE(vm_name) \
typedef struct {           \
    vm_uint32_t handle;    \
} vm_name##_h;

#endif /* VM_PLATFORM_H */
