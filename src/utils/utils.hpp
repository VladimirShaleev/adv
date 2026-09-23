#ifndef UTILS_UTILS_HPP
#define UTILS_UTILS_HPP

#if defined(_MSC_VER)
#define UTILS_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define UTILS_FORCE_INLINE __attribute__((always_inline)) inline
#else
#define UTILS_FORCE_INLINE inline
#endif

#include "object.hpp"
#include "string-pool.hpp"

#endif // UTILS_UTILS_HPP
