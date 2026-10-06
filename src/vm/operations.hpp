#ifndef VM_OPERATIONS_HPP
#define VM_OPERATIONS_HPP

#include "vm.hpp"

namespace vm {

struct BitwiseNot {
    static constexpr bool isUnary = true;
    static constexpr bool isLogic = false;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept { return type == VM_VALUE_TYPE_INT; }
    [[nodiscard]] static vm_sint32_t op(vm_sint32_t a, vm_sint32_t /* b */, utils::StringPool& /* pool */) noexcept {
        return ~a;
    }
    template <typename T>
    [[nodiscard]] static T op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct LogicalNot {
    static constexpr bool isUnary = true;
    static constexpr bool isLogic = true;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept { return type == VM_VALUE_TYPE_BOOL; }
    [[nodiscard]] static bool op(vm_bool_t a, vm_bool_t /* b */, utils::StringPool& /* pool */) noexcept {
        return !static_cast<bool>(a);
    }
    template <typename T>
    [[nodiscard]] static bool op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct NumericNegation {
    static constexpr bool isUnary = true;
    static constexpr bool isLogic = false;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept {
        return type == VM_VALUE_TYPE_INT || type == VM_VALUE_TYPE_FLOAT;
    }
    [[nodiscard]] static vm_sint32_t op(vm_sint32_t a, vm_sint32_t /* b */, utils::StringPool& /* pool */) noexcept {
        return -a;
    }
    [[nodiscard]] static vm_float32_t op(vm_float32_t a, vm_float32_t /* b */, utils::StringPool& /* pool */) noexcept {
        return -a;
    }
    template <typename T>
    [[nodiscard]] static T op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct And {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = false;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept { return type == VM_VALUE_TYPE_INT; }
    [[nodiscard]] static vm_sint32_t op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept {
        return a & b;
    }
    template <typename T>
    [[nodiscard]] static T op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct Or {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = false;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept { return type == VM_VALUE_TYPE_INT; }
    [[nodiscard]] static vm_sint32_t op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept {
        return a | b;
    }
    template <typename T>
    [[nodiscard]] static T op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct Xor {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = false;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept { return type == VM_VALUE_TYPE_INT; }
    [[nodiscard]] static vm_sint32_t op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept {
        return a ^ b;
    }
    template <typename T>
    [[nodiscard]] static T op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct LogicShiftLeft {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = false;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept { return type == VM_VALUE_TYPE_INT; }
    [[nodiscard]] static vm_sint32_t op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept {
        return static_cast<vm_uint32_t>(a) << static_cast<vm_uint32_t>(b);
    }
    template <typename T>
    [[nodiscard]] static T op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct LogicShiftRight {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = false;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept { return type == VM_VALUE_TYPE_INT; }
    [[nodiscard]] static vm_sint32_t op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept {
        return static_cast<vm_uint32_t>(a) >> static_cast<vm_uint32_t>(b);
    }
    template <typename T>
    [[nodiscard]] static T op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct ArithmeticShiftLeft {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = false;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept { return type == VM_VALUE_TYPE_INT; }
    [[nodiscard]] static vm_sint32_t op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept {
        return a << b;
    }
    template <typename T>
    [[nodiscard]] static T op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct ArithmeticShiftRight {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = false;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept { return type == VM_VALUE_TYPE_INT; }
    [[nodiscard]] static vm_sint32_t op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept {
        return a >> b;
    }
    template <typename T>
    [[nodiscard]] static T op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct Add {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = false;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept {
        return type == VM_VALUE_TYPE_INT || type == VM_VALUE_TYPE_FLOAT || type == VM_VALUE_TYPE_STRING;
    }
    [[nodiscard]] static vm_sint32_t op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept {
        return a + b;
    }
    [[nodiscard]] static vm_float32_t op(vm_float32_t a, vm_float32_t b, utils::StringPool& /* pool */) noexcept {
        return a + b;
    }
    [[nodiscard]] static vm_string_h op(vm_string_h a, vm_string_h b, utils::StringPool& pool) noexcept {
        auto strA = pool[a];
        auto strB = pool[b];
        const auto len = strA.length() + strB.length();
        if (len < 128) {
            char buffer[128];
            std::memcpy(buffer, strA.data(), strA.length());
            std::memcpy(buffer + strA.length(), strB.data(), strB.length());
            std::string_view str{buffer, len};
            return pool.insert(str);
        } else {
            std::string str;
            str.reserve(len);
            str += strA;
            str += strB;
            return pool.insert(str);
        }
    }
    template <typename T>
    [[nodiscard]] static T op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct Sub {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = false;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept {
        return type == VM_VALUE_TYPE_INT || type == VM_VALUE_TYPE_FLOAT;
    }
    [[nodiscard]] static vm_sint32_t op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept {
        return a - b;
    }
    [[nodiscard]] static vm_float32_t op(vm_float32_t a, vm_float32_t b, utils::StringPool& /* pool */) noexcept {
        return a - b;
    }
    template <typename T>
    [[nodiscard]] static T op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct Mul {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = false;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept {
        return type == VM_VALUE_TYPE_INT || type == VM_VALUE_TYPE_FLOAT;
    }
    [[nodiscard]] static vm_sint32_t op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept {
        return a * b;
    }
    [[nodiscard]] static vm_float32_t op(vm_float32_t a, vm_float32_t b, utils::StringPool& /* pool */) noexcept {
        return a * b;
    }
    template <typename T>
    [[nodiscard]] static T op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct Div {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = false;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept {
        return type == VM_VALUE_TYPE_INT || type == VM_VALUE_TYPE_FLOAT;
    }
    [[nodiscard]] static vm_sint32_t op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept {
        return a / b;
    }
    [[nodiscard]] static vm_float32_t op(vm_float32_t a, vm_float32_t b, utils::StringPool& /* pool */) noexcept {
        return a / b;
    }
    template <typename T>
    [[nodiscard]] static T op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct Mod {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = false;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept { return type == VM_VALUE_TYPE_INT; }
    [[nodiscard]] static vm_sint32_t op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept {
        return a % b;
    }
    template <typename T>
    [[nodiscard]] static T op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct Equal {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = true;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept { return true; }
    template <typename T>
    [[nodiscard]] static bool op(T a, T b, utils::StringPool& /* pool */) noexcept {
        return a == b;
    }
};

struct NotEqual {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = true;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept { return true; }
    template <typename T>
    [[nodiscard]] static bool op(T a, T b, utils::StringPool& /* pool */) noexcept {
        return a != b;
    }
};

struct Less {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = true;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept {
        return type == VM_VALUE_TYPE_INT || type == VM_VALUE_TYPE_FLOAT || type == VM_VALUE_TYPE_STRING;
    }
    [[nodiscard]] static bool op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept { return a < b; }
    [[nodiscard]] static bool op(vm_float32_t a, vm_float32_t b, utils::StringPool& /* pool */) noexcept {
        return a < b;
    }
    [[nodiscard]] static bool op(vm_string_h a, vm_string_h b, utils::StringPool& pool) noexcept {
        if (a == b) {
            return false;
        }
        return pool[a] < pool[b];
    }
    template <typename T>
    [[nodiscard]] static bool op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct LessEqual {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = true;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept {
        return type == VM_VALUE_TYPE_INT || type == VM_VALUE_TYPE_FLOAT || type == VM_VALUE_TYPE_STRING;
    }
    [[nodiscard]] static bool op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept {
        return a <= b;
    }
    [[nodiscard]] static bool op(vm_float32_t a, vm_float32_t b, utils::StringPool& /* pool */) noexcept {
        return a <= b;
    }
    [[nodiscard]] static bool op(vm_string_h a, vm_string_h b, utils::StringPool& pool) noexcept {
        if (a == b) {
            return true;
        }
        return pool[a] <= pool[b];
    }
    template <typename T>
    [[nodiscard]] static bool op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct Great {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = true;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept {
        return type == VM_VALUE_TYPE_INT || type == VM_VALUE_TYPE_FLOAT || type == VM_VALUE_TYPE_STRING;
    }
    [[nodiscard]] static bool op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept { return a > b; }
    [[nodiscard]] static bool op(vm_float32_t a, vm_float32_t b, utils::StringPool& /* pool */) noexcept {
        return a > b;
    }
    [[nodiscard]] static bool op(vm_string_h a, vm_string_h b, utils::StringPool& pool) noexcept {
        if (a == b) {
            return false;
        }
        return pool[a] > pool[b];
    }
    template <typename T>
    [[nodiscard]] static bool op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

struct GreatEqual {
    static constexpr bool isUnary = false;
    static constexpr bool isLogic = true;
    [[nodiscard]] static bool allowed(vm_value_type_t type) noexcept {
        return type == VM_VALUE_TYPE_INT || type == VM_VALUE_TYPE_FLOAT || type == VM_VALUE_TYPE_STRING;
    }
    [[nodiscard]] static bool op(vm_sint32_t a, vm_sint32_t b, utils::StringPool& /* pool */) noexcept {
        return a >= b;
    }
    [[nodiscard]] static bool op(vm_float32_t a, vm_float32_t b, utils::StringPool& /* pool */) noexcept {
        return a >= b;
    }
    [[nodiscard]] static bool op(vm_string_h a, vm_string_h b, utils::StringPool& pool) noexcept {
        if (a == b) {
            return true;
        }
        return pool[a] >= pool[b];
    }
    template <typename T>
    [[nodiscard]] static bool op(T /* a */, T /* b */, utils::StringPool& /* pool */) noexcept {
        return {};
    }
};

} // namespace vm

#endif // VM_OPERATIONS_HPP
