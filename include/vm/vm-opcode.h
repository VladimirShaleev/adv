/**
 * @file      vm-opcode.h
 * @brief     Instruction opcodes for the virtual machine.
 * @details   Defines the operations encoded in VM script bytecode.
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   vm_files
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
 * @ingroup vm_enums
 */
typedef enum
{
    VM_OPCODE_NO_OPERATION           = 0, /**< Performs no operation. */

    /* Imports a module */
    VM_OPCODE_IMPORT                 = 1, /**< Imports an external module. */

    /* Literals / Values Push */
    VM_OPCODE_PUSH_TRUE              = 2, /**< Pushes boolean true onto the stack. */
    VM_OPCODE_PUSH_FALSE             = 3, /**< Pushes boolean false onto the stack. */
    VM_OPCODE_PUSH_INT               = 4, /**< Pushes an integer literal onto the stack. */
    VM_OPCODE_PUSH_FLOAT             = 5, /**< Pushes a floating-point literal onto the stack. */
    VM_OPCODE_PUSH_STRING_FROM_DATA  = 6, /**< Pushes a string literal onto the stack from the data block. */
    VM_OPCODE_PUSH_STRING            = 7, /**< Pushes a string literal onto the stack. */

    /* Variables & Memory Access */
    VM_OPCODE_PUSH_GLOBAL_BY_NAME    = 8, /**< Pushes the value of a global variable identified by name. */
    VM_OPCODE_PUSH_GLOBAL            = 9, /**< Pushes the value of a global variable identified by its operand. */
    VM_OPCODE_PUSH_LOCAL             = 10, /**< Pushes the value of a local variable identified by its operand. */
    VM_OPCODE_PUSH_OUTER_LOCAL       = 11, /**< Pushes the value of an outer local variable from an enclosing stack frame (depth, slot). */
    VM_OPCODE_POP_GLOBAL_BY_NAME     = 12, /**< Stores the stack top in the global variable identified by name. */
    VM_OPCODE_POP_GLOBAL             = 13, /**< Stores the stack top in the global variable identified by its operand. */
    VM_OPCODE_POP_LOCAL              = 14, /**< Stores the stack top in the local variable identified by its operand. */
    VM_OPCODE_POP_OUTER_LOCAL        = 15, /**< Stores the stack top in an outer local variable in an enclosing stack frame (depth, slot). */
    VM_OPCODE_POP                    = 16, /**< Removes the stack top. */

    /* Explicit / Implicit Type Casts */
    VM_OPCODE_BOOL_TO_INT            = 17, /**< Converts the stack-top boolean to an integer. */
    VM_OPCODE_BOOL_TO_FLOAT          = 18, /**< Converts the stack-top boolean to a float. */
    VM_OPCODE_BOOL_TO_STRING         = 19, /**< Converts the stack-top boolean to a string. */
    VM_OPCODE_INT_TO_BOOL            = 20, /**< Converts the stack-top integer to a boolean. */
    VM_OPCODE_INT_TO_FLOAT           = 21, /**< Converts the stack-top integer to a float. */
    VM_OPCODE_INT_TO_STRING          = 22, /**< Converts the stack-top integer to a string. */
    VM_OPCODE_FLOAT_TO_BOOL          = 23, /**< Converts the stack-top float to a boolean. */
    VM_OPCODE_FLOAT_TO_INT           = 24, /**< Converts the stack-top float to an integer. */
    VM_OPCODE_FLOAT_TO_STRING        = 25, /**< Converts the stack-top float to a string. */
    VM_OPCODE_STRING_TO_BOOL         = 26, /**< Converts the stack-top string to a boolean. */
    VM_OPCODE_STRING_TO_INT          = 27, /**< Converts the stack-top string to an integer. */
    VM_OPCODE_STRING_TO_FLOAT        = 28, /**< Converts the stack-top string to a float. */

    /* Unary Operators */
    VM_OPCODE_BITWISE_NOT            = 29, /**< Applies bitwise NOT to the stack top. */
    VM_OPCODE_LOGICAL_NOT            = 30, /**< Applies logical NOT to the stack top. */
    VM_OPCODE_NUMERIC_NEGATION       = 31, /**< Negates the numeric stack top. */

    /* Bitwise Logical & Shifts Operators */
    VM_OPCODE_BITWISE_AND            = 32, /**< Applies bitwise AND to the top two stack values. */
    VM_OPCODE_BITWISE_OR             = 33, /**< Applies bitwise OR to the top two stack values. */
    VM_OPCODE_BITWISE_XOR            = 34, /**< Applies bitwise XOR to the top two stack values. */
    VM_OPCODE_LOGIC_SHIFT_LEFT       = 35, /**< Logically shifts the left operand left by the right operand's count. */
    VM_OPCODE_LOGIC_SHIFT_RIGHT      = 36, /**< Logically shifts the left operand right by the right operand's count. */
    VM_OPCODE_ARITHMETIC_SHIFT_LEFT  = 37, /**< Arithmetically shifts the left operand left by the right operand's count. */
    VM_OPCODE_ARITHMETIC_SHIFT_RIGHT = 38, /**< Arithmetically shifts the left operand right by the right operand's count. */

    /* Arithmetic Binary Operators */
    VM_OPCODE_ADD                    = 39, /**< Adds the top two stack values. */
    VM_OPCODE_SUBTRACT               = 40, /**< Subtracts the top stack value from the preceding value. */
    VM_OPCODE_MULTIPLY               = 41, /**< Multiplies the top two stack values. */
    VM_OPCODE_DIVISION               = 42, /**< Divides the preceding stack value by the top value. */
    VM_OPCODE_MOD                    = 43, /**< Computes the remainder of the preceding value divided by the top value. */

    /* Comparison Operators */
    VM_OPCODE_EQUAL                  = 44, /**< Compares the top two stack values for equality. */
    VM_OPCODE_NOT_EQUAL              = 45, /**< Compares the top two stack values for inequality. */
    VM_OPCODE_LESS                   = 46, /**< Tests whether the preceding value is less than the top value. */
    VM_OPCODE_LESS_EQUAL             = 47, /**< Tests whether the preceding value is less than or equal to the top value. */
    VM_OPCODE_GREAT                  = 48, /**< Tests whether the preceding value is greater than the top value. */
    VM_OPCODE_GREAT_EQUAL            = 49, /**< Tests whether the preceding value is greater than or equal to the top value. */

    /* Subroutines & Execution Control */
    VM_OPCODE_CALL_NATIVE_BY_NAME    = 50, /**< Calls a native function by name. */
    VM_OPCODE_CALL_NATIVE            = 51, /**< Calls a native function by its operand. */
    VM_OPCODE_CALL_BY_NAME           = 52, /**< Calls a subroutine identified by name. */
    VM_OPCODE_CALL                   = 53, /**< Calls a subroutine identified by its operand. */
    VM_OPCODE_RETURN                 = 54, /**< Returns from the current subroutine. */

    /* Jumps & Control Flow */
    VM_OPCODE_JUMP                   = 55, /**< Unconditionally jumps to the target identified by its operand. */
    VM_OPCODE_JUMP_IF_FALSE          = 56, /**< Jumps to the target if the stack-top condition is false. */

    /* Concurrency & VM Lifecycle */
    VM_OPCODE_YIELD                  = 57, /**< Yields execution so another VM thread can run. */
    VM_OPCODE_WAIT                   = 58, /**< Suspends the current execution until its wait condition is met. */
    VM_OPCODE_HALT                   = 59, /**< Halts the current VM execution. */

    VM_OPCODE_COUNT_ENUMS            = 60, /**< Count of constants in the enumeration. */
    VM_OPCODE_MAX_ENUM               = 0x7FFFFFFF /**< Max value of enum (not used). */
} vm_opcode_t;

#ifdef __cplusplus
}
#endif

#endif /* VM_OPCODE_H */
