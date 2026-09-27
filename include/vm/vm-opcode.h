#ifndef VM_OPCODE_H
#define VM_OPCODE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    VM_OPCODE_NO_OPERATION,

    // --- Literals / Values Push ---
    VM_OPCODE_PUSH_ZERO,
    VM_OPCODE_PUSH_TRUE,
    VM_OPCODE_PUSH_FALSE,
    VM_OPCODE_PUSH_INT,
    VM_OPCODE_PUSH_FLOAT,
    VM_OPCODE_PUSH_STRING,

    // --- Variables & Memory Access ---
    VM_OPCODE_PUSH_GLOBAL_BY_NAME,
    VM_OPCODE_PUSH_GLOBAL,
    VM_OPCODE_PUSH_LOCAL,
    VM_OPCODE_POP_GLOBAL_BY_NAME,
    VM_OPCODE_POP_GLOBAL,
    VM_OPCODE_POP_LOCAL,
    VM_OPCODE_POP,

    // --- Explicit / Implicit Type Casts ---
    VM_OPCODE_BOOL_TO_INT,
    VM_OPCODE_BOOL_TO_FLOAT,
    VM_OPCODE_BOOL_TO_STRING,
    VM_OPCODE_INT_TO_BOOL,
    VM_OPCODE_INT_TO_FLOAT,
    VM_OPCODE_INT_TO_STRING,
    VM_OPCODE_FLOAT_TO_BOOL,
    VM_OPCODE_FLOAT_TO_INT,
    VM_OPCODE_FLOAT_TO_STRING,
    VM_OPCODE_STRING_TO_BOOL,
    VM_OPCODE_STRING_TO_INT,
    VM_OPCODE_STRING_TO_FLOAT,

    // --- Unary Operators ---
    VM_OPCODE_BITWISE_NOT,
    VM_OPCODE_LOGICAL_NOT,
    VM_OPCODE_NUMERIC_NEGATION,

    // --- Bitwise Shifts ---
    VM_OPCODE_LOGIC_SHIFT_LEFT,
    VM_OPCODE_LOGIC_SHIFT_RIGHT,
    VM_OPCODE_ARITHMETIC_SHIFT_LEFT,
    VM_OPCODE_ARITHMETIC_SHIFT_RIGHT,

    // --- Bitwise Logical Operators ---
    VM_OPCODE_BITWISE_AND,
    VM_OPCODE_BITWISE_OR,
    VM_OPCODE_BITWISE_XOR,

    // --- Arithmetic Binary Operators ---
    VM_OPCODE_ADD,
    VM_OPCODE_SUBTRACT,
    VM_OPCODE_MULTIPLY,
    VM_OPCODE_DIVISION,
    VM_OPCODE_MOD,

    // --- Comparison Operators ---
    VM_OPCODE_EQUAL,
    VM_OPCODE_NOT_EQUAL,
    VM_OPCODE_LESS,
    VM_OPCODE_LESS_EQUAL,
    VM_OPCODE_GREAT,
    VM_OPCODE_GREAT_EQUAL,

    // --- Subroutines & Execution Control ---
    VM_OPCODE_CALL_NATIVE,
    VM_OPCODE_CALL_BY_NAME,
    VM_OPCODE_CALL,
    VM_OPCODE_RETURN,

    // --- Jumps & Control Flow ---
    VM_OPCODE_JUMP,
    VM_OPCODE_JUMP_IF_FALSE,

    // --- Concurrency & VM Lifecycle ---
    VM_OPCODE_YIELD,
    VM_OPCODE_WAIT,
    VM_OPCODE_HALT
} vm_opcode_t;

#ifdef __cplusplus
}
#endif

#endif /* VM_OPCODE_H */
