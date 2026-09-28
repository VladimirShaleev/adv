#ifndef VM_SCRIPT_HPP
#define VM_SCRIPT_HPP

#include "vm.hpp"

namespace vm {

constexpr vm_script_h UndefinedScript{std::numeric_limits<decltype(vm_script_h::handle)>::max()};

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

struct ScriptThreadHandle {
    vm_uint16_t handle;
};

struct ScriptThread {
    using Handle = ScriptThreadHandle;

    ThreadState state;
    uint32_t scriptId;
    uint32_t ip;
    uint32_t waitTicks;

    // vm_value_t stack[STACK_SIZE];
    int32_t sp;

    // CallFrame frames[MAX_FRAMES];
    int32_t frameCount;
};

} // namespace vm

#endif // VM_SCRIPT_HPP
