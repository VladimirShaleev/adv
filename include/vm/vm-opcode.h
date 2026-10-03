/**
 * @file      vm-opcode.h
 * @brief     Instruction opcodes for the virtual machine.
 * @details   Defines the operations encoded in VM script bytecode.
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   files
 * @copyright MIT License
 */
#ifndef VM_OPCODE_H
#define VM_OPCODE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Virtual machine instruction opcodes.
 * @details Identifies operations encoded in a script's bytecode. Instructions
 *          may consume values from or push values onto the VM stack.
 * @ingroup enums
 */
typedef enum
{
    VM_OPCODE_NO_OPERATION           = 0, /**< Performs no operation. */

    /* Literals / Values Push */
    VM_OPCODE_PUSH_TRUE              = 1, /**< Pushes boolean true onto the stack. */
    VM_OPCODE_PUSH_FALSE             = 2, /**< Pushes boolean false onto the stack. */
    VM_OPCODE_PUSH_INT               = 3, /**< Pushes an integer literal onto the stack. */
    VM_OPCODE_PUSH_FLOAT             = 4, /**< Pushes a floating-point literal onto the stack. */
    VM_OPCODE_PUSH_STRING            = 5, /**< Pushes a string literal onto the stack. */

    /* Variables & Memory Access */
    VM_OPCODE_PUSH_GLOBAL_BY_NAME    = 6, /**< Pushes the value of a global variable identified by name. */
    VM_OPCODE_PUSH_GLOBAL            = 7, /**< Pushes the value of a global variable identified by its operand. */
    VM_OPCODE_PUSH_LOCAL             = 8, /**< Pushes the value of a local variable identified by its operand. */
    VM_OPCODE_POP_GLOBAL_BY_NAME     = 9, /**< Stores the stack top in the global variable identified by name. */
    VM_OPCODE_POP_GLOBAL             = 10, /**< Stores the stack top in the global variable identified by its operand. */
    VM_OPCODE_POP_LOCAL              = 11, /**< Stores the stack top in the local variable identified by its operand. */
    VM_OPCODE_POP                    = 12, /**< Removes the stack top. */

    /* Explicit / Implicit Type Casts */
    VM_OPCODE_BOOL_TO_INT            = 13, /**< Converts the stack-top boolean to an integer. */
    VM_OPCODE_BOOL_TO_FLOAT          = 14, /**< Converts the stack-top boolean to a float. */
    VM_OPCODE_BOOL_TO_STRING         = 15, /**< Converts the stack-top boolean to a string. */
    VM_OPCODE_INT_TO_BOOL            = 16, /**< Converts the stack-top integer to a boolean. */
    VM_OPCODE_INT_TO_FLOAT           = 17, /**< Converts the stack-top integer to a float. */
    VM_OPCODE_INT_TO_STRING          = 18, /**< Converts the stack-top integer to a string. */
    VM_OPCODE_FLOAT_TO_BOOL          = 19, /**< Converts the stack-top float to a boolean. */
    VM_OPCODE_FLOAT_TO_INT           = 20, /**< Converts the stack-top float to an integer. */
    VM_OPCODE_FLOAT_TO_STRING        = 21, /**< Converts the stack-top float to a string. */
    VM_OPCODE_STRING_TO_BOOL         = 22, /**< Converts the stack-top string to a boolean. */
    VM_OPCODE_STRING_TO_INT          = 23, /**< Converts the stack-top string to an integer. */
    VM_OPCODE_STRING_TO_FLOAT        = 24, /**< Converts the stack-top string to a float. */

    /* Unary Operators */
    VM_OPCODE_BITWISE_NOT            = 25, /**< Applies bitwise NOT to the stack top. */
    VM_OPCODE_LOGICAL_NOT            = 26, /**< Applies logical NOT to the stack top. */
    VM_OPCODE_NUMERIC_NEGATION       = 27, /**< Negates the numeric stack top. */

    /* Bitwise Logical & Shifts Operators */
    VM_OPCODE_BITWISE_AND            = 28, /**< Applies bitwise AND to the top two stack values. */
    VM_OPCODE_BITWISE_OR             = 29, /**< Applies bitwise OR to the top two stack values. */
    VM_OPCODE_BITWISE_XOR            = 30, /**< Applies bitwise XOR to the top two stack values. */
    VM_OPCODE_LOGIC_SHIFT_LEFT       = 31, /**< Logically shifts the left operand left by the right operand's count. */
    VM_OPCODE_LOGIC_SHIFT_RIGHT      = 32, /**< Logically shifts the left operand right by the right operand's count. */
    VM_OPCODE_ARITHMETIC_SHIFT_LEFT  = 33, /**< Arithmetically shifts the left operand left by the right operand's count. */
    VM_OPCODE_ARITHMETIC_SHIFT_RIGHT = 34, /**< Arithmetically shifts the left operand right by the right operand's count. */

    /* Arithmetic Binary Operators */
    VM_OPCODE_ADD                    = 35, /**< Adds the top two stack values. */
    VM_OPCODE_SUBTRACT               = 36, /**< Subtracts the top stack value from the preceding value. */
    VM_OPCODE_MULTIPLY               = 37, /**< Multiplies the top two stack values. */
    VM_OPCODE_DIVISION               = 38, /**< Divides the preceding stack value by the top value. */
    VM_OPCODE_MOD                    = 39, /**< Computes the remainder of the preceding value divided by the top value. */

    /* Comparison Operators */
    VM_OPCODE_EQUAL                  = 40, /**< Compares the top two stack values for equality. */
    VM_OPCODE_NOT_EQUAL              = 41, /**< Compares the top two stack values for inequality. */
    VM_OPCODE_LESS                   = 42, /**< Tests whether the preceding value is less than the top value. */
    VM_OPCODE_LESS_EQUAL             = 43, /**< Tests whether the preceding value is less than or equal to the top value. */
    VM_OPCODE_GREAT                  = 44, /**< Tests whether the preceding value is greater than the top value. */
    VM_OPCODE_GREAT_EQUAL            = 45, /**< Tests whether the preceding value is greater than or equal to the top value. */

    /* Subroutines & Execution Control */
    VM_OPCODE_CALL_NATIVE            = 46, /**< Calls a native function. */
    VM_OPCODE_CALL_BY_NAME           = 47, /**< Calls a subroutine identified by name. */
    VM_OPCODE_CALL                   = 48, /**< Calls a subroutine identified by its operand. */
    VM_OPCODE_RETURN                 = 49, /**< Returns from the current subroutine. */

    /* Jumps & Control Flow */
    VM_OPCODE_JUMP                   = 50, /**< Unconditionally jumps to the target identified by its operand. */
    VM_OPCODE_JUMP_IF_FALSE          = 51, /**< Jumps to the target if the stack-top condition is false. */

    /* Concurrency & VM Lifecycle */
    VM_OPCODE_YIELD                  = 52, /**< Yields execution so another VM thread can run. */
    VM_OPCODE_WAIT                   = 53, /**< Suspends the current execution until its wait condition is met. */
    VM_OPCODE_HALT                   = 54, /**< Halts the current VM execution. */

    VM_OPCODE_COUNT_ENUMS            = 55, /**< Count of constants in the enumeration. */
    VM_OPCODE_MAX_ENUM               = 0x7FFFFFFF /**< Max value of enum (not used). */
} vm_opcode_t;

#ifdef __cplusplus
}
#endif

#endif /* VM_OPCODE_H */
