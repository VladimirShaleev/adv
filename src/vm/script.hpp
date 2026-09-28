#ifndef VM_SCRIPT_HPP
#define VM_SCRIPT_HPP

#include "vm.hpp"

namespace vm {

constexpr size_t MAX_THREADS = 16;
constexpr size_t STACK_SIZE = 64;
constexpr size_t LOCALS_COUNT = 16;
constexpr size_t GLOBALS_COUNT = 256;
constexpr size_t MAX_NATIVE_FUNCTIONS = 64;
constexpr size_t MAX_FRAMES = 16;

enum class ThreadState {
    Dead = 0,
    Running = 1,
    Waiting = 2
};

struct CallFrame {
    vm_uint32_t returnIp;
    vm_uint32_t prevScriptId;
    vm_uint32_t argCount;
    vm_uint32_t baseSp;
};

struct ScriptThread {
    ThreadState state;
    uint32_t scriptId;
    uint32_t ip;
    uint32_t waitTicks;

    vm_value_t stack[STACK_SIZE];
    int32_t sp;

    CallFrame frames[MAX_FRAMES];
    int32_t frameCount;
};

} // namespace vm

#endif // VM_SCRIPT_HPP
