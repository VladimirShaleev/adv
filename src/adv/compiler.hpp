#ifndef ADV_COMPILER_HPP
#define ADV_COMPILER_HPP

#ifndef ADV_STATIC_BUILD
#ifdef _MSC_VER
#define ADV_API __declspec(dllexport)
#elif __GNUC__ >= 4
#define ADV_API __attribute__((visibility("default")))
#else
#define ADV_API
#endif
#endif

#if defined(_MSC_VER)
#define ADV_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define ADV_FORCE_INLINE __attribute__((always_inline)) inline
#else
#define ADV_FORCE_INLINE inline
#endif

#include <adv/adv.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <utils/string-pool.hpp>

#endif // ADV_COMPILER_HPP
