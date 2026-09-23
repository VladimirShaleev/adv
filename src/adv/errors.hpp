#ifndef ADV_ERRORS_HPP
#define ADV_ERRORS_HPP

#include "compiler.hpp"

namespace adv {

template <adv_status_t Status, typename... Args>
inline std::string err(Args&&... args) {
    std::string str;
    if constexpr (Status == ADV_STATUS_N1001) {
        str = fmt::format("Symbol '{}' name shadowing in an outer scope (defined at {})", args...);
    } else if constexpr (Status == ADV_STATUS_W2001) {
        str = fmt::format("{} with value {} out of range [{}, {}]", args...);
    } else if constexpr (Status == ADV_STATUS_W2002) {
        str = fmt::format("Implicit conversion from '{}' to '{}' in return statement may lose precision", args...);
    } else if constexpr (Status == ADV_STATUS_W2003) {
        str = fmt::format("Implicit conversion from '{}' to '{}' in binary operation may lose precision", args...);
    } else if constexpr (Status == ADV_STATUS_W2004) {
        str = fmt::format(
            "Implicit conversion from '{}' to '{}' for argument '{}' in call to '{}' may lose precision", args...);
    } else if constexpr (Status == ADV_STATUS_W2005) {
        str = fmt::format("Implicit conversion from '{}' to '{}' in assignment may lose precision", args...);
    } else if constexpr (Status == ADV_STATUS_W2006) {
        str = fmt::format(
            "Implicit conversion from '{}' to '{}' in variable '{}' initialization may lose precision", args...);
    } else if constexpr (Status == ADV_STATUS_E3001) {
        if constexpr (sizeof...(args) > 0) {
            str = fmt::format("Syntax error '{}'", args...);
        } else {
            str = fmt::format("Syntax error");
        }
    } else if constexpr (Status == ADV_STATUS_E3002) {
        str = fmt::format("Unknown error");
    } else if constexpr (Status == ADV_STATUS_E3003) {
        str = fmt::format("String closing character not found in string: {}", args...);
    } else if constexpr (Status == ADV_STATUS_E3004) {
        str = fmt::format("Unterminated block comment");
    } else if constexpr (Status == ADV_STATUS_E3005) {
        str = fmt::format("Invalid escape character, {}", args...);
    } else if constexpr (Status == ADV_STATUS_E3006) {
        str = fmt::format("Failed to infer the argument '{}' type", args...);
    } else if constexpr (Status == ADV_STATUS_E3007) {
        str = fmt::format("Variable '{}' declaration requires a type annotation or an initializer", args...);
    } else if constexpr (Status == ADV_STATUS_E3008) {
        str = fmt::format("Symbol '{}' is already declared in this scope", args...);
    } else if constexpr (Status == ADV_STATUS_E3009) {
        str = fmt::format("Symbol '{}' not found", args...);
    } else if constexpr (Status == ADV_STATUS_E3010) {
        str = fmt::format("Unknown target type '{}' in explicit cast expression", args...);
    } else if constexpr (Status == ADV_STATUS_E3011) {
        str = fmt::format("Unknown return type '{}' in declaration of function '{}'", args...);
    } else if constexpr (Status == ADV_STATUS_E3012) {
        str = fmt::format("Unknown target type '{}' in explicit cast expression", args...);
    } else if constexpr (Status == ADV_STATUS_E3013) {
        str = fmt::format("Function '{}' with no return type cannot return a value", args...);
    } else if constexpr (Status == ADV_STATUS_E3014) {
        str = fmt::format("Non-void function '{}' must return a value", args...);
    } else if constexpr (Status == ADV_STATUS_E3015) {
        str =
            fmt::format("Cannot convert return expression of type '{}' to return type '{}' in function '{}'", args...);
    } else if constexpr (Status == ADV_STATUS_E3016) {
        str = fmt::format("Parameter '{}' must have an explicit type annotation", args...);
    } else if constexpr (Status == ADV_STATUS_E3017) {
        str = fmt::format("Unknown type '{}' specified for parameter '{}'", args...);
    } else if constexpr (Status == ADV_STATUS_E3018) {
        str = fmt::format("Variable or symbol '{}' is used before its type is resolved", args...);
    } else if constexpr (Status == ADV_STATUS_E3019) {
        str = fmt::format("Invalid operands to binary expression, incompatible types '{}' and '{}'", args...);
    } else if constexpr (Status == ADV_STATUS_E3020) {
        str = fmt::format("Call to undeclared function '{}'", args...);
    } else if constexpr (Status == ADV_STATUS_E3021) {
        str = fmt::format("Symbol '{}' is not a function and cannot be called", args...);
    } else if constexpr (Status == ADV_STATUS_E3022) {
        str = fmt::format("Function '{}' expects {} argument(s), but {} were provided", args...);
    } else if constexpr (Status == ADV_STATUS_E3023) {
        str =
            fmt::format("Cannot convert argument '{}' from type '{}' to parameter type '{}' in call to '{}'", args...);
    } else if constexpr (Status == ADV_STATUS_E3024) {
        str = fmt::format("Cannot assign value of type '{}' to target of type '{}'", args...);
    } else if constexpr (Status == ADV_STATUS_E3025) {
        str = fmt::format(
            "Variable declaration '{}' must have either an explicit type annotation or an initializer", args...);
    } else if constexpr (Status == ADV_STATUS_E3026) {
        str = fmt::format("Unknown type '{}' in declaration of variable '{}'", args...);
    } else if constexpr (Status == ADV_STATUS_E3027) {
        str = fmt::format("Cannot initialize variable '{}' of type '{}' with expression of type '{}'", args...);
    } else {
        assert(!"Unknown status code");
    }
    return str;
}

} // namespace adv

#endif // ADV_ERRORS_HPP
