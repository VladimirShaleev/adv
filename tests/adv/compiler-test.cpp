#include <adv/adv.h>
#include <vm/vm.h>

#include <gtest/gtest.h>

#include <string>

namespace {

void captureOutput(const adv_source_t* source, adv_data_t data) {
    auto& output = *static_cast<std::string*>(data);
    output.assign(source->data, source->size);
}

struct ReturnCheck {
    bool called{};
    vm_value_t value{};
};

void verifyReturnedStructureField(vm_t vm, vm_thread_h thread, vm_native_h, vm_uint16_t, vm_data_t data) {
    auto& result = *static_cast<ReturnCheck*>(data);
    result.called = true;
    result.value = vm_thread_get_arg(vm, thread, 0);
}

TEST(Compiler, ExportsOnlyGlobalStructures) {
    constexpr char sourceText[] =
        "struct Child { value: int; }\n"
        "struct Global { child: Child; }\n"
        "function func() {\n"
        "    struct Local { value: int; }\n"
        "}\n";
    adv_source_t source{"test.adv", sourceText, static_cast<adv_uint32_t>(sizeof(sourceText) - 1)};

    adv_options_t options{};
    ASSERT_EQ(adv_options_create(&options), ADV_RESULT_SUCCESS);
    adv_options_set_standard_library(options, 0);
    adv_options_set_output_format(options, ADV_FORMAT_TEXT);

    std::string output;
    adv_options_set_writer(options, captureOutput, &output);

    adv_compiler_t compiler{};
    const auto createResult = adv_compiler_create(&compiler);
    if (createResult != ADV_RESULT_SUCCESS) {
        adv_options_destroy(options);
        FAIL() << adv_result_to_string(createResult);
    }

    const auto compileResult = adv_compiler_compile(compiler, nullptr, 1, &source, options, nullptr);
    adv_compiler_destroy(compiler);
    adv_options_destroy(options);

    ASSERT_EQ(compileResult, ADV_RESULT_SUCCESS) << adv_result_to_string(compileResult);
    EXPECT_NE(output.find("struct Global { child: Child; }"), std::string::npos);
    EXPECT_EQ(output.find("struct Local"), std::string::npos);
}

TEST(Compiler, GeneratesStructureFieldAccess) {
    constexpr char sourceText[] =
        "struct Point { x: float; y: float; }\n"
        "struct Sample { name: string; p: Point; }\n"
        "var global: Sample;\n"
        "global.p.y = 3.5;\n"
        "var value: float;\n"
        "value = global.p.y;\n"
        "function outer() {\n"
        "    var local: Sample;\n"
        "    local.p.y = 1.5;\n"
        "    value = local.p.y;\n"
        "    function inner() {\n"
        "        local.p.y = 2.5;\n"
        "        value = local.p.y;\n"
        "    }\n"
        "}\n";
    adv_source_t source{"test.adv", sourceText, static_cast<adv_uint32_t>(sizeof(sourceText) - 1)};

    adv_options_t options{};
    ASSERT_EQ(adv_options_create(&options), ADV_RESULT_SUCCESS);
    adv_options_set_standard_library(options, 0);
    adv_options_set_output_format(options, ADV_FORMAT_TEXT);

    std::string output;
    adv_options_set_writer(options, captureOutput, &output);

    adv_compiler_t compiler{};
    const auto createResult = adv_compiler_create(&compiler);
    if (createResult != ADV_RESULT_SUCCESS) {
        adv_options_destroy(options);
        FAIL() << adv_result_to_string(createResult);
    }

    const auto compileResult = adv_compiler_compile(compiler, nullptr, 1, &source, options, nullptr);
    adv_compiler_destroy(compiler);
    adv_options_destroy(options);

    EXPECT_EQ(compileResult, ADV_RESULT_SUCCESS) << adv_result_to_string(compileResult);
    EXPECT_NE(output.find("pop_global global.1.1"), std::string::npos);
    EXPECT_NE(output.find("push_global global.1.1"), std::string::npos);
    EXPECT_NE(output.find("pop_local 2"), std::string::npos);
    EXPECT_NE(output.find("push_local 2"), std::string::npos);
    EXPECT_NE(output.find("pop_outer 1 2"), std::string::npos);
    EXPECT_NE(output.find("push_outer 1 2"), std::string::npos);
}

TEST(Compiler, FlattensStructureArgumentsForCalls) {
    constexpr char sourceText[] =
        "struct Point { x: int; y: int; }\n"
        "struct Sample { id: int; point: Point; }\n"
        "var source: Sample;\n"
        "function consume(before: int, value: Sample, after: float) {}\n"
        "native native_consume(value: Sample);\n"
        "consume(555, source, 3.5);\n"
        "native_consume(source);\n"
        "function outer() {\n"
        "    var local: Sample;\n"
        "    function inner() {\n"
        "        consume(555, local, 3.5);\n"
        "    }\n"
        "}\n";
    adv_source_t source{"test.adv", sourceText, static_cast<adv_uint32_t>(sizeof(sourceText) - 1)};

    adv_options_t options{};
    ASSERT_EQ(adv_options_create(&options), ADV_RESULT_SUCCESS);
    adv_options_set_standard_library(options, 0);
    adv_options_set_output_format(options, ADV_FORMAT_TEXT);

    std::string output;
    adv_options_set_writer(options, captureOutput, &output);

    adv_compiler_t compiler{};
    const auto createResult = adv_compiler_create(&compiler);
    if (createResult != ADV_RESULT_SUCCESS) {
        adv_options_destroy(options);
        FAIL() << adv_result_to_string(createResult);
    }

    const auto compileResult = adv_compiler_compile(compiler, nullptr, 1, &source, options, nullptr);
    adv_compiler_destroy(compiler);
    adv_options_destroy(options);

    ASSERT_EQ(compileResult, ADV_RESULT_SUCCESS) << adv_result_to_string(compileResult);
    const auto firstArg = output.find("push 555");
    const auto firstField = output.find("push_global source.0", firstArg);
    const auto secondField = output.find("push_global source.1.0", firstField);
    const auto thirdField = output.find("push_global source.1.1", secondField);
    const auto lastArg = output.find("push 3.5", thirdField);
    const auto functionCall = output.find("call consume", lastArg);
    ASSERT_NE(firstArg, std::string::npos);
    ASSERT_NE(firstField, std::string::npos);
    ASSERT_NE(secondField, std::string::npos);
    ASSERT_NE(thirdField, std::string::npos);
    ASSERT_NE(lastArg, std::string::npos);
    ASSERT_NE(functionCall, std::string::npos);

    const auto nativeFirstField = output.find("push_global source.0", functionCall);
    const auto nativeSecondField = output.find("push_global source.1.0", nativeFirstField);
    const auto nativeThirdField = output.find("push_global source.1.1", nativeSecondField);
    const auto nativeCall = output.find("call_native native_consume", nativeThirdField);
    EXPECT_NE(nativeFirstField, std::string::npos);
    EXPECT_NE(nativeSecondField, std::string::npos);
    EXPECT_NE(nativeThirdField, std::string::npos);
    EXPECT_NE(nativeCall, std::string::npos);

    EXPECT_NE(output.find("push_outer 1 0"), std::string::npos);
    EXPECT_NE(output.find("push_outer 1 1"), std::string::npos);
    EXPECT_NE(output.find("push_outer 1 2"), std::string::npos);
}

TEST(Compiler, CopiesStructureSlotsInDeclarationsAndAssignments) {
    constexpr char sourceText[] =
        "struct Inner { first: int; second: int; }\n"
        "struct Sample { id: int; inner: Inner; }\n"
        "var source: Sample;\n"
        "var copy = source;\n"
        "var copy_2: Sample;\n"
        "copy_2 = source;\n"
        "function copyLocals() {\n"
        "    var local: Sample;\n"
        "    var localCopy = local;\n"
        "    var localCopy_2: Sample;\n"
        "    localCopy_2 = local;\n"
        "}\n";
    adv_source_t source{"test.adv", sourceText, static_cast<adv_uint32_t>(sizeof(sourceText) - 1)};

    adv_options_t options{};
    ASSERT_EQ(adv_options_create(&options), ADV_RESULT_SUCCESS);
    adv_options_set_standard_library(options, 0);
    adv_options_set_output_format(options, ADV_FORMAT_TEXT);

    std::string output;
    adv_options_set_writer(options, captureOutput, &output);

    adv_compiler_t compiler{};
    const auto createResult = adv_compiler_create(&compiler);
    if (createResult != ADV_RESULT_SUCCESS) {
        adv_options_destroy(options);
        FAIL() << adv_result_to_string(createResult);
    }

    const auto compileResult = adv_compiler_compile(compiler, nullptr, 1, &source, options, nullptr);
    adv_compiler_destroy(compiler);
    adv_options_destroy(options);

    ASSERT_EQ(compileResult, ADV_RESULT_SUCCESS) << adv_result_to_string(compileResult);
    const auto globalSource0 = output.find("push_global source.0");
    const auto globalSource1 = output.find("push_global source.1.0", globalSource0);
    const auto globalSource2 = output.find("push_global source.1.1", globalSource1);
    const auto globalDest2 = output.find("pop_global copy.1.1", globalSource2);
    const auto globalDest1 = output.find("pop_global copy.1.0", globalDest2);
    const auto globalDest0 = output.find("pop_global copy.0", globalDest1);
    ASSERT_NE(globalSource0, std::string::npos);
    ASSERT_NE(globalSource1, std::string::npos);
    ASSERT_NE(globalSource2, std::string::npos);
    ASSERT_NE(globalDest2, std::string::npos);
    ASSERT_NE(globalDest1, std::string::npos);
    ASSERT_NE(globalDest0, std::string::npos);

    const auto assignmentSource0 = output.find("push_global source.0", globalDest0);
    const auto assignmentSource1 = output.find("push_global source.1.0", assignmentSource0);
    const auto assignmentSource2 = output.find("push_global source.1.1", assignmentSource1);
    const auto assignmentDest2 = output.find("pop_global copy_2.1.1", assignmentSource2);
    const auto assignmentDest1 = output.find("pop_global copy_2.1.0", assignmentDest2);
    const auto assignmentDest0 = output.find("pop_global copy_2.0", assignmentDest1);
    ASSERT_NE(assignmentSource0, std::string::npos);
    ASSERT_NE(assignmentSource1, std::string::npos);
    ASSERT_NE(assignmentSource2, std::string::npos);
    ASSERT_NE(assignmentDest2, std::string::npos);
    ASSERT_NE(assignmentDest1, std::string::npos);
    ASSERT_NE(assignmentDest0, std::string::npos);

    const auto localSource0 = output.find("push_local 0", assignmentDest0);
    const auto localSource1 = output.find("push_local 1", localSource0);
    const auto localSource2 = output.find("push_local 2", localSource1);
    const auto localDest2 = output.find("pop_local 5", localSource2);
    const auto localDest1 = output.find("pop_local 4", localDest2);
    const auto localDest0 = output.find("pop_local 3", localDest1);
    EXPECT_NE(localSource0, std::string::npos);
    EXPECT_NE(localSource1, std::string::npos);
    EXPECT_NE(localSource2, std::string::npos);
    EXPECT_NE(localDest2, std::string::npos);
    EXPECT_NE(localDest1, std::string::npos);
    EXPECT_NE(localDest0, std::string::npos);

    const auto localAssignmentSource0 = output.find("push_local 0", localDest0);
    const auto localAssignmentSource1 = output.find("push_local 1", localAssignmentSource0);
    const auto localAssignmentSource2 = output.find("push_local 2", localAssignmentSource1);
    const auto localAssignmentDest2 = output.find("pop_local 8", localAssignmentSource2);
    const auto localAssignmentDest1 = output.find("pop_local 7", localAssignmentDest2);
    const auto localAssignmentDest0 = output.find("pop_local 6", localAssignmentDest1);
    EXPECT_NE(localAssignmentSource0, std::string::npos);
    EXPECT_NE(localAssignmentSource1, std::string::npos);
    EXPECT_NE(localAssignmentSource2, std::string::npos);
    EXPECT_NE(localAssignmentDest2, std::string::npos);
    EXPECT_NE(localAssignmentDest1, std::string::npos);
    EXPECT_NE(localAssignmentDest0, std::string::npos);
}

TEST(Compiler, ReturnsStructureSlotsAndSelectsCallFields) {
    constexpr char sourceText[] =
        "struct Inner { first: int; second: int; }\n"
        "struct Sample { id: int; inner: Inner; }\n"
        "struct Tiny { value: int; }\n"
        "var global_source: Sample;\n"
        "function make(value: Sample): Sample { return value; }\n"
        "function makeTiny(): Tiny { var value: Tiny; value.value = 4; return value; }\n"
        "var global_result = make(global_source).inner.second;\n"
        "function getValue(): int {\n"
        "    var source: Sample;\n"
        "    source.id = 1;\n"
        "    source.inner.first = 2;\n"
        "    source.inner.second = 3;\n"
        "    var copy = make(source);\n"
        "    var tiny_value = makeTiny().value;\n"
        "    return make(copy).inner.second;\n"
        "}\n"
        "native verify(value: int);\n"
        "verify(getValue());\n";
    adv_source_t source{"test.adv", sourceText, static_cast<adv_uint32_t>(sizeof(sourceText) - 1)};

    adv_options_t options{};
    ASSERT_EQ(adv_options_create(&options), ADV_RESULT_SUCCESS);
    adv_options_set_standard_library(options, 0);

    std::string bytecode;
    adv_options_set_writer(options, captureOutput, &bytecode);

    adv_compiler_t compiler{};
    const auto createResult = adv_compiler_create(&compiler);
    if (createResult != ADV_RESULT_SUCCESS) {
        adv_options_destroy(options);
        FAIL() << adv_result_to_string(createResult);
    }

    const auto compileResult = adv_compiler_compile(compiler, nullptr, 1, &source, options, nullptr);
    adv_compiler_destroy(compiler);
    adv_options_destroy(options);
    ASSERT_EQ(compileResult, ADV_RESULT_SUCCESS) << adv_result_to_string(compileResult);

    vm_script_export_native_t nativeExport{};
    ASSERT_NE(
        vm_read_script_export_native(
            bytecode.data(), static_cast<vm_uint32_t>(bytecode.size()), 0, &nativeExport),
        nullptr);
    EXPECT_EQ(nativeExport.param_count, 1);
    vm_script_export_param_t nativeParam{};
    ASSERT_NE(
        vm_read_script_export_native_param(
            bytecode.data(), static_cast<vm_uint32_t>(bytecode.size()), 0, 0, &nativeParam),
        nullptr);
    EXPECT_EQ(nativeParam.type, VM_VALUE_TYPE_INT);
    vm_script_export_t scriptExport{};
    ASSERT_NE(
        vm_read_script_export(
            bytecode.data(), static_cast<vm_uint32_t>(bytecode.size()), &scriptExport),
        nullptr);
    EXPECT_EQ(scriptExport.root_frame_slots, 3);

    vm_t vm{};
    ASSERT_EQ(vm_create(&vm), VM_RESULT_SUCCESS);
    ReturnCheck result{};
    ASSERT_NE(vm_register_native(vm, "verify", verifyReturnedStructureField, &result).handle, 0);
    const auto script = vm_load_script_from_memory(vm, bytecode.data(), static_cast<vm_uint32_t>(bytecode.size()));
    ASSERT_NE(script.handle, 0);
    ASSERT_NE(vm_run_thread(vm, script).handle, 0);
    vm_tick(vm, 16);
    vm_tick(vm, 16);
    vm_destroy(vm);

    ASSERT_TRUE(result.called);
    EXPECT_EQ(result.value.type, VM_VALUE_TYPE_INT);
    EXPECT_EQ(result.value.i32, 3);
}

} // namespace
