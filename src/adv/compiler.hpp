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

#include <adv/adv.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <utils/utils.hpp>

constexpr vm_value_type_t VM_VALUE_TYPE_VOID = vm_value_type_t(int(VM_VALUE_TYPE_COUNT_ENUMS) + 1);

namespace utils {
template <>
struct Result<adv_result_t> {
    static constexpr adv_result_t Success = ADV_RESULT_SUCCESS;
    static constexpr adv_result_t Error = ADV_RESULT_ERROR_UNKNOWN;
    static constexpr adv_result_t OutOfMemory = ADV_RESULT_ERROR_OUT_OF_MEMORY;
};
} // namespace utils

#endif // ADV_COMPILER_HPP
