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

    vm_utf8_t dirs[] = {"D:\\Development\\Projects\\adv\\build\\"};
    vm_set_import_dirs(vm, std::size(dirs), dirs);

    auto script = vm_load_script_from_file(vm, "D:\\Development\\Projects\\adv\\build\\test.adb");
    EXPECT_NE(script, Undefined);

    auto thread = vm_run_script(vm, script);
    EXPECT_NE(thread, Undefined);

    vm_tick(vm);
    vm_tick(vm);

    auto thread2 = vm_run_script(vm, script);
    EXPECT_NE(thread2, Undefined);
    vm_tick(vm);
}
