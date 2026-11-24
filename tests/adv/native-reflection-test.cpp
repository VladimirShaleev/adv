#include <adv/adv.h>
#include <vm/vm.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace {

void captureOutput(const adv_source_t* source, adv_data_t data) {
    auto& output = *static_cast<std::string*>(data);
    output.assign(source->data, source->size);
}

struct ReflectionResult {
    bool called{};
    bool valid{};
    bool returned{};
    vm_sint32_t returnedValue{};
};

struct VmGuard {
    vm_t vm{};

    ~VmGuard() { vm_destroy(vm); }
};

void inspectNative(vm_t vm, vm_thread_h thread, vm_native_h native, vm_uint16_t argCount, vm_data_t data) {
    auto& result = *static_cast<ReflectionResult*>(data);
    result.called = true;

    const auto* func = vm_reflect_native_func(vm, native);
    if (!func || argCount != 3 || func->param_count != 3 || !func->return_type) {
        return;
    }

    const auto& paramType = func->params[1].type;
    const auto* outer = paramType.structure;
    if (!outer || std::string_view(outer->name) != "Outer" || outer->field_count != 2 || outer->slot_count != 3) {
        return;
    }

    const auto* inner = outer->fields[0].type.structure;
    if (!inner || std::string_view(inner->name) != "Inner" || inner->field_count != 2 || inner->slot_count != 2) {
        return;
    }

    const auto prefix = vm_thread_get_arg(vm, thread, 0);
    const auto count = vm_thread_get_arg(vm, thread, 1);
    const auto extra = vm_thread_get_arg(vm, thread, 2);
    const auto childActive = vm_thread_get_arg(vm, thread, 3);
    const auto active = vm_thread_get_arg(vm, thread, 4);
    result.valid = std::string_view(outer->fields[0].name) == "child" &&
                   std::string_view(inner->fields[0].name) == "count" &&
                   inner->fields[0].type.primitive_type == VM_VALUE_TYPE_INT &&
                   inner->fields[0].slot_index == 0 && inner->fields[0].slot_count == 1 &&
                   inner->fields[1].slot_index == 1 && inner->fields[1].slot_count == 1 &&
                   func->params[1].slot_index == 1 && func->params[1].slot_count == 3 &&
                   func->params[0].slot_index == 0 && func->params[0].slot_count == 1 &&
                   outer->fields[0].slot_index == 0 && outer->fields[0].slot_count == 2 &&
                   std::string_view(outer->fields[1].name) == "active" &&
                   outer->fields[1].type.primitive_type == VM_VALUE_TYPE_BOOL &&
                   outer->fields[1].slot_index == 2 && outer->fields[1].slot_count == 1 &&
                   func->params[2].slot_index == 4 && func->params[2].slot_count == 1 &&
                   func->return_type->structure == outer &&
                   func->return_slot_count == outer->slot_count &&
                   prefix.type == VM_VALUE_TYPE_INT && prefix.i32 == 7 &&
                   count.type == VM_VALUE_TYPE_INT && count.i32 == 42 &&
                   extra.type == VM_VALUE_TYPE_FLOAT && extra.f32 == 1.5f &&
                   childActive.type == VM_VALUE_TYPE_BOOL && childActive.b &&
                   active.type == VM_VALUE_TYPE_BOOL && !active.b;

    const vm_value_t returnCount{.type = VM_VALUE_TYPE_INT, .i32 = 1};
    const vm_value_t returnExtra{.type = VM_VALUE_TYPE_FLOAT, .f32 = 2.5f};
    const vm_value_t returnActive{.type = VM_VALUE_TYPE_BOOL, .b = true};
    vm_thread_set_result(vm, thread, 0, &returnCount);
    vm_thread_set_result(vm, thread, 1, &returnExtra);
    vm_thread_set_result(vm, thread, 2, &returnActive);
}

void verifyNativeResult(vm_t vm, vm_thread_h thread, vm_native_h, vm_uint16_t, vm_data_t data) {
    auto& result = *static_cast<ReflectionResult*>(data);
    result.returned = true;
    result.returnedValue = vm_thread_get_arg(vm, thread, 0).i32;
}

TEST(VM, ReflectsNestedNativeParameterAndReturnStructures) {
    constexpr char sourceText[] =
        "struct Inner { count: int; extra: float; }\n"
        "struct Outer { child: Inner; active: bool; }\n"
        "native inspect(prefix: int, value: Outer, active: bool): Outer;\n"
        "native verify(value: int);\n"
        "var value: Outer;\n"
        "value.child.count = 42;\n"
        "value.child.extra = 1.5;\n"
        "value.active = true;\n"
        "var result = inspect(7, value, false);\n"
        "verify(result.child.count);\n";
    adv_source_t source{"reflection.adv", sourceText, static_cast<adv_uint32_t>(sizeof(sourceText) - 1)};

    adv_options_t options{};
    ASSERT_EQ(adv_options_create(&options), ADV_RESULT_SUCCESS);
    adv_options_set_standard_library(options, 0);
    adv_options_set_output_format(options, ADV_FORMAT_BINARY);

    std::string bytecode;
    adv_options_set_writer(options, captureOutput, &bytecode);

    adv_compiler_t compiler{};
    ASSERT_EQ(adv_compiler_create(&compiler), ADV_RESULT_SUCCESS);
    const auto compileResult = adv_compiler_compile(compiler, nullptr, 1, &source, options, nullptr);
    adv_compiler_destroy(compiler);
    adv_options_destroy(options);
    ASSERT_EQ(compileResult, ADV_RESULT_SUCCESS);

    VmGuard guard;
    ASSERT_EQ(vm_create(&guard.vm), VM_RESULT_SUCCESS);
    const auto vm = guard.vm;
    ReflectionResult reflection;
    vm_register_native(vm, "inspect", inspectNative, &reflection);
    vm_register_native(vm, "verify", verifyNativeResult, &reflection);

    const auto script = vm_load_script_from_memory(
        vm, bytecode.data(), static_cast<vm_uint32_t>(bytecode.size()));
    ASSERT_NE(script.handle, 0);
    const auto thread = vm_run_thread(vm, script);
    ASSERT_NE(thread.handle, 0);
    for (int i = 0; i < 4; ++i) {
        vm_tick(vm, 16);
    }

    EXPECT_TRUE(reflection.called);
    EXPECT_TRUE(reflection.valid);
    EXPECT_TRUE(reflection.returned);
    EXPECT_EQ(reflection.returnedValue, 1);
}

} // namespace
