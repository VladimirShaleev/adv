#include <compare>
#include <gtest/gtest.h>
#include <limits>

#include <vm/vm.h>

#include "finally.hpp"

[[nodiscard]] static inline bool operator==(vm_script_h lhs, vm_script_h rhs) noexcept {
    return lhs.handle == rhs.handle;
}

[[nodiscard]] static inline bool operator!=(vm_script_h lhs, vm_script_h rhs) noexcept {
    return lhs.handle != rhs.handle;
}

[[nodiscard]] static inline std::strong_ordering operator<=>(vm_script_h lhs, vm_script_h rhs) noexcept {
    return lhs.handle <=> rhs.handle;
}

static constexpr vm_script_h Undefined = vm_script_h{};

// TODO: temporary test
TEST(VM, Test) {
    vm_t vm{};
    deferred(vm_destroy(vm));
    const auto result = vm_create(&vm);
    EXPECT_EQ(result, VM_RESULT_SUCCESS);

    auto script = vm_load_script_from_file(vm, "D:\\Development\\Projects\\adv\\build\\test.adb");
    EXPECT_NE(script, Undefined);
}
