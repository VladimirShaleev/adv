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
    vm_script_h prevScript;
    vm_uint32_t argCount;
    vm_uint32_t baseStackPos;
    vm_uint32_t staticFrameIndex;
    bool hasReturnValue;
};

struct ScriptThread {
    using Handle = vm_thread_h;

    ThreadState state;
    vm_script_h script;
    vm_uint32_t instructionPos;
    vm_uint32_t waitTicks;
    Handle waitThread;
    vm_uint32_t stackPos;
    vm_uint32_t frameCount;
    vm_native_h currentNative;
    vm_value_t stack[StackSize];
    CallFrame frames[MaxFrames];
};

using ScriptThreadPool = utils::Pool<ScriptThread>;

} // namespace vm

#endif // VM_SCRIPT_HPP
