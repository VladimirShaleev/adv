#ifndef VM_SCRIPT_HPP
#define VM_SCRIPT_HPP

#include "vm.hpp"

namespace vm {

constexpr size_t StackSize = 256;
constexpr size_t MaxFrames = 32;

enum class ThreadState {
    Dead = 0,
    Running = 1,
    Waiting = 2
};

struct CallFrame {
    vm_uint32_t returnInstructionPos;
    vm_uint32_t prevScriptId;
    vm_uint32_t argCount;
    vm_uint32_t baseStackPos;
    bool hasReturnValue;
};

struct ScriptThreadHandle {
    vm_uint16_t handle;
};

struct ScriptThread {
    using Handle = ScriptThreadHandle;

    ThreadState state;
    vm_script_h script;
    vm_uint32_t instructionPos;
    vm_uint32_t waitTicks;
    vm_uint32_t stackPos;
    vm_uint32_t frameCount;
    vm_value_t stack[StackSize];
    CallFrame frames[MaxFrames];
};

} // namespace vm

#endif // VM_SCRIPT_HPP
