#include <gtest/gtest.h>
#include <utils/utils.hpp>

#include <vm/vm.h>

#include "finally.hpp"

// TODO: temporary test
TEST(VM, Test) {
    vm_t vm{};
    deferred(vm_destroy(vm));
    const auto result = vm_create(&vm);
    EXPECT_EQ(result, VM_RESULT_SUCCESS);

    vm_utf8_t dirs[] = {"D:\\Development\\Projects\\adv\\build\\win64-debug\\"};
    vm_set_import_dirs(vm, std::size(dirs), dirs);

    auto script = vm_load_script_from_file(vm, "D:\\Development\\Projects\\adv\\build\\win64-debug\\test.adb");
    EXPECT_NE(script, Undefined);

    vm_register_native(
        vm,
        "c_func",
        [](vm_t vm, vm_thread_h thread, vm_native_h native, vm_uint16_t arg_count, vm_data_t data) {
            const auto a = vm_thread_get_arg(vm, thread, 0);
            const auto b = vm_thread_get_arg(vm, thread, 1);
            assert(a.type == VM_VALUE_TYPE_FLOAT);
            assert(b.type == VM_VALUE_TYPE_STRING);
            vm_uint32_t len{};
            auto strPtr = vm_get_string(vm, b.str, &len);
            std::string_view str{strPtr, len};
            std::cout << str << std::endl;

            auto resultStr = std::to_string(a.f32) + ' ' + std::string(str);
            vm_value_t result{.type = VM_VALUE_TYPE_STRING, .str = vm_intern_string(vm, resultStr.c_str())};
            vm_thread_set_result(vm, thread, 0, &result);
        },
        nullptr);

    vm_register_native(
        vm,
        "sec",
        [](vm_t vm, vm_thread_h thread, vm_native_h native, vm_uint16_t arg_count, vm_data_t data) {
            std::time_t now = std::time(nullptr);
            std::tm* local_time = std::localtime(&now);
            vm_value_t result{};
            result.type = VM_VALUE_TYPE_INT;
            result.i32 = local_time->tm_sec;
            vm_thread_set_result(vm, thread, 0, &result);
        },
        nullptr);

    vm_set_max_iterations_per_step(vm, 1);
    auto thread = vm_run_thread(vm, script);
    EXPECT_NE(thread, Undefined);

    while (vm_tick(vm, 16)) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(8ms);
    }
}
