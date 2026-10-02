/**
 * @file      adv-platform.h
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
 * @ingroup   files
 * @copyright MIT License
 */
#ifndef ADV_PLATFORM_H
#define ADV_PLATFORM_H

/**
 * @def     ADV_API
 * @brief   Controls symbol visibility for shared library builds.
 * @details This macro is used to control symbol visibility when building or using the library.
 *          On Windows (**MSVC**) with dynamic linking (non-static build), it expands to `__declspec(dllimport)`.
 *          In all other cases (static builds or non-Windows platforms), it expands to nothing.
 *          This allows proper importing of symbols from DLLs on Windows platforms.
 * @note    Define `ADV_STATIC_BUILD` for static library configuration.
 * @ingroup macros
 */

#ifndef ADV_API
# if defined(_MSC_VER) && !defined(ADV_STATIC_BUILD)
#  define ADV_API __declspec(dllimport)
# else
#  define ADV_API
# endif
#endif

#if defined(_WIN32) && !defined(ADV_PLATFORM_WINDOWS)
# define ADV_PLATFORM_WINDOWS
#elif defined(__APPLE__)
# include <TargetConditionals.h>
# include <unistd.h>
# if TARGET_OS_IPHONE && !defined(ADV_PLATFORM_IOS)
#  define ADV_PLATFORM_IOS
# elif TARGET_IPHONE_SIMULATOR && !defined(ADV_PLATFORM_IOS)
#  define ADV_PLATFORM_IOS
# elif TARGET_OS_MAC && !defined(ADV_PLATFORM_MAC_OS)
#  define ADV_PLATFORM_MAC_OS
# else
#  error unsupported Apple platform
# endif
#elif defined(__ANDROID__) && !defined(ADV_PLATFORM_ANDROID)
# define ADV_PLATFORM_ANDROID
#elif defined(__linux__) && !defined(ADV_PLATFORM_LINUX)
# define ADV_PLATFORM_LINUX
#elif defined(__EMSCRIPTEN__) && !defined(ADV_PLATFORM_WEB)
# define ADV_PLATFORM_WEB
#else
# error unsupported platform
#endif

/**
 * @addtogroup types Types
 * @{
 */

/**
 * @name  Platform-independent type definitions.
 * @brief Fixed-size types guaranteed to work across all supported platforms.
 * @{
 */
#include <stdint.h>
typedef char        adv_char_t;    /**< symbol type. */
typedef int32_t     adv_bool_t;    /**< boolean type. */
typedef int8_t      adv_sint8_t;   /**< 8 bit signed integer. */
typedef uint8_t     adv_uint8_t;   /**< 8 bit unsigned integer. */
typedef int16_t     adv_sint16_t;  /**< 16 bit signed integer. */
typedef uint16_t    adv_uint16_t;  /**< 16 bit unsigned integer. */
typedef int32_t     adv_sint32_t;  /**< 32 bit signed integer. */
typedef uint32_t    adv_uint32_t;  /**< 32 bit unsigned integer. */
typedef int64_t     adv_sint64_t;  /**< 64 bit signed integer. */
typedef uint64_t    adv_uint64_t;  /**< 64 bit unsigned integer. */
typedef float       adv_float32_t; /**< 32 bit float point. */
typedef double      adv_float64_t; /**< 64 bit float point. */
typedef const char* adv_utf8_t;    /**< utf8 string. */
typedef void*       adv_data_t;    /**< pointer to data. */
/** @} */

/** @} */

/**
 * @def       ADV_TYPE
 * @brief     Declares an opaque handle type.
 * @details   Creates a typedef for a pointer to an incomplete struct type,
 *            providing type safety while hiding implementation details.
 * @param[in] adv_name Base name for the type (suffix `_t` will be added).
 * @ingroup   macros
 */
#define ADV_TYPE(adv_name) \
typedef struct _##adv_name* adv_name##_t;

/**
 * @def       ADV_HANDLE
 * @brief     Declares an index-based handle type.
 * @details   Creates a struct containing an index value, typically used for
 *            resource handles in API designs that avoid direct pointers.
 * @param[in] adv_name Base name for the handle type (suffix `_h` will be added).
 */
#define ADV_HANDLE(adv_name) \
typedef struct {             \
    adv_uint16_t handle;     \
} adv_name##_h;

#endif /* ADV_PLATFORM_H */
