#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <utils/translator.hpp>
#include <utils/utils.hpp>
#include <vm/vm-script.h>
#include <vm/vm-value.h>
#include <vm/vm-version.h>
#include <vm/vm.h>

namespace {

std::vector<char> makeBinary(
    vm_uint32_t dataOffset, vm_uint32_t dataSize, vm_uint32_t textOffset, vm_uint32_t textSize, size_t fileSize = 64) {
    std::vector<char> code(fileSize);
    vm_script_header_t header{{'a', 'd', 'v', 'b'}, 0, 0};
    vm_script_table_t table{0, 0, dataOffset, dataSize, textOffset, textSize};
    std::memcpy(code.data(), &header, sizeof(header));
    std::memcpy(code.data() + sizeof(header), &table, sizeof(table));
    return code;
}

} // namespace

TEST(Translator, BinarySectionsDoNotOverlap) {
    constexpr auto sectionsOffset = sizeof(vm_script_header_t) + sizeof(vm_script_table_t);
    auto code = makeBinary(sectionsOffset, 8, sectionsOffset + 8, 8);

    EXPECT_TRUE(utils::isBinaryCode(code));
}

TEST(Translator, TextSectionBeforeDataSection) {
    constexpr auto sectionsOffset = sizeof(vm_script_header_t) + sizeof(vm_script_table_t);
    auto code = makeBinary(sectionsOffset + 8, 8, sectionsOffset, 8);

    EXPECT_TRUE(utils::isBinaryCode(code));
}

TEST(Translator, BinarySectionsSeparatedByGap) {
    constexpr auto sectionsOffset = sizeof(vm_script_header_t) + sizeof(vm_script_table_t);
    auto code = makeBinary(sectionsOffset, 8, sectionsOffset + 12, 8);

    EXPECT_TRUE(utils::isBinaryCode(code));
}

TEST(Translator, BinarySectionsOverlap) {
    constexpr auto sectionsOffset = sizeof(vm_script_header_t) + sizeof(vm_script_table_t);
    auto code = makeBinary(sectionsOffset, 10, sectionsOffset + 8, 8);

    EXPECT_FALSE(utils::isBinaryCode(code));
}

TEST(Translator, BinarySectionExtendsPastEndOfFile) {
    constexpr auto sectionsOffset = sizeof(vm_script_header_t) + sizeof(vm_script_table_t);
    auto code = makeBinary(sectionsOffset, 8, 60, 8);

    EXPECT_FALSE(utils::isBinaryCode(code));
}

TEST(Translator, TranslatesNasmlikeSectionsAndInstructions) {
    const std::string source =
        ".header\n"
        "    format: ADVT\n"
        "    version: 1.0.0\n"
        "\n"
        ".text\n"
        "    import other_module\n"
        "    push 3\n"
        "    push_global g_test\n"
        "    call sum\n"
        "    pop_global ok\n"
        "    jump label\n"
        "label:\n"
        "    halt\n";

    utils::TranslateResult result{};
    const auto bytecode = utils::translateTextToBinary(source, result);

    EXPECT_EQ(result.errorCode, utils::TranslateResult::NoError) << result.message;
    EXPECT_EQ(result.format, utils::Format::Advt);
    EXPECT_FALSE(bytecode.empty());
}

TEST(Translator, ParsesFunctionProloguesAsLabels) {
    const std::string source =
        ".header\n"
        "    format: ADVT\n"
        "    version: 1.0.0\n"
        ".export\n"
        "    module test 0\n"
        "    function func(): bool\n"
        ".text\n"
        "    call func\n"
        "    halt\n"
        ".function func (rets: 0, depth: 0, slots: 1, params: 0):\n"
        "    ret\n"
        ".function @inner:\n"
        "    ret\n";

    utils::TranslateResult result{};
    const auto bytecode = utils::translateTextToBinary(source, result);

    EXPECT_EQ(result.errorCode, utils::TranslateResult::NoError) << result.message;
    EXPECT_FALSE(bytecode.empty());
    EXPECT_EQ(utils::read<vm_script_export_func_t>(bytecode, 0).text_offset, 6);
}

TEST(Translator, RejectsNegativeFunctionPrologueValues) {
    const std::string source =
        ".header\n"
        "    format: ADVT\n"
        "    version: 1.0.0\n"
        ".text\n"
        ".function func (slots: -1):\n"
        "    ret\n";

    utils::TranslateResult result{};
    utils::translateTextToBinary(source, result);

    EXPECT_EQ(result.errorCode, utils::TranslateResult::SyntaxError);
    EXPECT_EQ(result.line, 5);
}

TEST(Translator, ResolvesUncalledExportedFunctionOffset) {
    const std::string source =
        ".header\n"
        "    format: ADVT\n"
        "    version: 1.0.0\n"
        ".export\n"
        "    module other_module 0\n"
        "    var g_test: int\n"
        "    native export_native(value: bool)\n"
        "    function sum(a: int, b: int): int\n"
        ".text\n"
        "    push 2\n"
        "    pop_global g_test\n"
        "    jump label\n"
        ".function sum (slots: 2):\n"
        "    push_local 0\n"
        "    push_local 1\n"
        "    add\n"
        "    ret\n"
        "label:\n"
        "    halt\n";

    utils::TranslateResult result{};
    const auto bytecode = utils::translateTextToBinary(source, result);
    ASSERT_EQ(result.errorCode, utils::TranslateResult::NoError) << result.message;

    const auto function = utils::read<vm_script_export_func_t>(bytecode, 0);
    EXPECT_EQ(function.text_offset, 15);
}

TEST(Translator, SerializesExportSymbols) {
    const std::string source =
        ".header\n"
        "    format: ADVT\n"
        "    version: 1.0.0\n"
        ".export\n"
        "    module test 0\n"
        "    var ok: int\n"
        "    var cret: string\n"
        "    native c_func(a: float, b: string): string\n"
        "    function myfunc(a: int, b: float): float\n"
        ".text\n"
        ".function myfunc (slots: 3):\n"
        "    halt\n";

    utils::TranslateResult result{};
    const auto bytecode = utils::translateTextToBinary(source, result);
    ASSERT_EQ(result.errorCode, utils::TranslateResult::NoError) << result.message;
    const std::span<const char> binary(
        reinterpret_cast<const char*>(bytecode.data()), bytecode.size());
    EXPECT_TRUE(utils::isBinaryCode(binary));
    EXPECT_TRUE(utils::validateChecksum(binary));

    const auto exports = utils::read<vm_script_export_t>(bytecode);
    const auto table = utils::read<vm_script_table_t>(bytecode);
    const auto data = utils::read<utils::SectionData>(bytecode).data;
    EXPECT_EQ(table.export_offset, sizeof(vm_script_header_t) + sizeof(vm_script_table_t));
    EXPECT_EQ(table.export_size, sizeof(vm_script_export_t) + 2 * sizeof(vm_script_export_var_t) +
                                     sizeof(vm_script_export_func_t) + 2 * sizeof(vm_script_export_param_t) +
                                     sizeof(vm_script_export_native_t) + 2 * sizeof(vm_script_export_param_t));
    EXPECT_EQ(table.data_offset, table.export_offset + table.export_size);
    EXPECT_EQ(table.text_offset, table.data_offset + table.data_size);
    EXPECT_EQ(utils::readString(exports.module_name, data), "test");
    EXPECT_EQ(exports.root_frame_slots, 0);
    ASSERT_EQ(exports.var_count, 2);
    ASSERT_EQ(exports.func_count, 1);
    ASSERT_EQ(exports.native_count, 1);

    const auto firstVar = utils::read<vm_script_export_var_t>(bytecode, 0);
    const auto secondVar = utils::read<vm_script_export_var_t>(bytecode, 1);
    EXPECT_EQ(utils::readString(firstVar.name, data), "ok");
    EXPECT_EQ(firstVar.type, VM_VALUE_TYPE_INT);
    EXPECT_EQ(utils::readString(secondVar.name, data), "cret");
    EXPECT_EQ(secondVar.type, VM_VALUE_TYPE_STRING);

    const auto function = utils::read<vm_script_export_func_t>(bytecode, 0);
    EXPECT_EQ(utils::readString(function.name, data), "myfunc");
    EXPECT_EQ(function.text_offset, 0);
    EXPECT_EQ(function.return_type, VM_VALUE_TYPE_FLOAT);
    ASSERT_EQ(function.param_count, 2);
    const auto firstFunctionParam = utils::read<vm_script_export_param_t>(bytecode, 0, 0);
    const auto secondFunctionParam = utils::read<vm_script_export_param_t>(bytecode, 0, 1);
    EXPECT_EQ(utils::readString(firstFunctionParam.name, data), "a");
    EXPECT_EQ(firstFunctionParam.type, VM_VALUE_TYPE_INT);
    EXPECT_EQ(utils::readString(secondFunctionParam.name, data), "b");
    EXPECT_EQ(secondFunctionParam.type, VM_VALUE_TYPE_FLOAT);

    const auto native = utils::read<vm_script_export_native_t>(bytecode, 0);
    EXPECT_EQ(utils::readString(native.name, data), "c_func");
    EXPECT_EQ(native.return_type, VM_VALUE_TYPE_STRING);
    ASSERT_EQ(native.param_count, 2);
    const auto firstNativeParam = utils::read<utils::NativeParam>(bytecode, 0, 0).param;
    const auto secondNativeParam = utils::read<utils::NativeParam>(bytecode, 0, 1).param;
    EXPECT_EQ(utils::readString(firstNativeParam.name, data), "a");
    EXPECT_EQ(firstNativeParam.type, VM_VALUE_TYPE_FLOAT);
    EXPECT_EQ(utils::readString(secondNativeParam.name, data), "b");
    EXPECT_EQ(secondNativeParam.type, VM_VALUE_TYPE_STRING);
}

TEST(Translator, SerializesStructureExportsAndDynamicTypes) {
    const std::string source =
        ".header\n"
        "    format: ADVT\n"
        "    version: " VM_VERSION_STRING "\n"
        ".export\n"
        "    module test 0\n"
        "    struct A { count: int; child: B; }\n"
        "    struct B {\n"
        "        enabled: bool;\n"
        "    }\n"
        "    var global: A\n"
        "    var zeta: int\n"
        "    function run(value: A, count: int): bool\n"
        "    function zetaRun(count: int): bool\n"
        "    native inspect(value: B)\n"
        "    native zetaNative(enabled: bool)\n"
        ".text\n"
        ".function run (slots: 2):\n"
        "    ret\n"
        ".function zetaRun (slots: 1):\n"
        "    ret\n";

    utils::TranslateResult result{};
    const auto bytecode = utils::translateTextToBinary(source, result);
    ASSERT_EQ(result.errorCode, utils::TranslateResult::NoError) << result.message;

    const auto exports = utils::read<vm_script_export_t>(bytecode);
    const auto data = utils::read<utils::SectionData>(bytecode).data;
    auto readBaseTypeName = [&](size_t recordOffset) {
        return utils::readString(utils::readExportBaseType(bytecode, recordOffset), data);
    };
    EXPECT_EQ(exports.struct_count, 2);
    EXPECT_EQ(exports.struct_size, 31);
    EXPECT_EQ(exports.var_size, 14);
    EXPECT_EQ(exports.func_size, 39);
    EXPECT_EQ(exports.native_size, 26);

    const auto structureA = utils::read<vm_script_export_struct_t>(bytecode, 0);
    const auto fieldChildOffset = utils::internal::Offset<vm_script_export_field_t>::offset(bytecode, 0, 1).first;
    const auto fieldChild = utils::read<vm_script_export_field_t>(bytecode, 0, 1);
    EXPECT_EQ(utils::readString(structureA.name, data), "A");
    EXPECT_EQ(fieldChild.type, VM_SCRIPT_TYPE_STRUCT);
    EXPECT_EQ(readBaseTypeName(fieldChildOffset), "B");

    const auto variableOffset = utils::internal::Offset<vm_script_export_var_t>::offset(bytecode, 0, 0).first;
    const auto variable = utils::read<vm_script_export_var_t>(bytecode, 0);
    EXPECT_EQ(variable.type, VM_SCRIPT_TYPE_STRUCT);
    EXPECT_EQ(readBaseTypeName(variableOffset), "A");
    const auto secondVariable = utils::read<vm_script_export_var_t>(bytecode, 1);
    EXPECT_EQ(utils::readString(secondVariable.name, data), "zeta");
    EXPECT_EQ(secondVariable.type, VM_SCRIPT_TYPE_INT);

    const auto function = utils::read<vm_script_export_func_t>(bytecode, 0);
    const auto firstParamOffset = utils::internal::Offset<vm_script_export_param_t>::offset(bytecode, 0, 0).first;
    const auto secondParamOffset = utils::internal::Offset<vm_script_export_param_t>::offset(bytecode, 0, 1).first;
    const auto firstParam = utils::read<vm_script_export_param_t>(bytecode, 0, 0);
    const auto secondParam = utils::read<vm_script_export_param_t>(bytecode, 0, 1);
    EXPECT_EQ(function.param_count, 2);
    EXPECT_EQ(firstParam.type, VM_SCRIPT_TYPE_STRUCT);
    EXPECT_EQ(readBaseTypeName(firstParamOffset), "A");
    EXPECT_EQ(secondParam.type, VM_SCRIPT_TYPE_INT);
    EXPECT_EQ(secondParamOffset, firstParamOffset + sizeof(firstParam) + sizeof(vm_uint32_t));
    const auto secondFunction = utils::read<vm_script_export_func_t>(bytecode, 1);
    EXPECT_EQ(utils::readString(secondFunction.name, data), "zetaRun");
    const auto secondFunctionParam = utils::read<vm_script_export_param_t>(bytecode, 1, 0);
    EXPECT_EQ(secondFunctionParam.type, VM_SCRIPT_TYPE_INT);

    const auto nativeParamOffset = utils::internal::Offset<utils::NativeParam>::offset(bytecode, 0, 0).first;
    const auto nativeParam = utils::read<utils::NativeParam>(bytecode, 0, 0).param;
    EXPECT_EQ(nativeParam.type, VM_SCRIPT_TYPE_STRUCT);
    EXPECT_EQ(readBaseTypeName(nativeParamOffset), "B");
    const auto secondNative = utils::read<vm_script_export_native_t>(bytecode, 1);
    EXPECT_EQ(utils::readString(secondNative.name, data), "zetaNative");
    const auto secondNativeParam = utils::read<utils::NativeParam>(bytecode, 1, 0).param;
    EXPECT_EQ(secondNativeParam.type, VM_SCRIPT_TYPE_BOOL);

    const auto* rawBuffer = reinterpret_cast<const vm_char_t*>(bytecode.data());
    vm_script_export_var_t apiVariable{};
    vm_script_export_struct_t apiStruct{};
    vm_script_export_field_t apiField{};
    vm_script_export_func_t apiFunction{};
    vm_script_export_native_t apiNative{};
    vm_script_export_param_t apiParam{};
    vm_uint32_t apiBaseType{};
    EXPECT_EQ(vm_read_script_export_var(
                  rawBuffer, static_cast<vm_uint32_t>(bytecode.size()), 0, &apiVariable, &apiBaseType),
              &apiVariable);
    EXPECT_EQ(utils::readString(apiVariable.name, data), "global");
    EXPECT_EQ(utils::readString(apiBaseType, data), "A");
    EXPECT_EQ(vm_read_script_export_struct(rawBuffer, static_cast<vm_uint32_t>(bytecode.size()), 0, &apiStruct),
              &apiStruct);
    EXPECT_EQ(utils::readString(apiStruct.name, data), "A");
    EXPECT_EQ(
        vm_read_script_export_struct_field(
            rawBuffer, static_cast<vm_uint32_t>(bytecode.size()), 0, 1, &apiField, &apiBaseType),
        &apiField);
    EXPECT_EQ(utils::readString(apiField.name, data), "child");
    EXPECT_EQ(utils::readString(apiBaseType, data), "B");
    EXPECT_EQ(
        vm_read_script_export_func_param(
            rawBuffer, static_cast<vm_uint32_t>(bytecode.size()), 0, 0, &apiParam, &apiBaseType),
        &apiParam);
    EXPECT_EQ(utils::readString(apiBaseType, data), "A");
    EXPECT_EQ(
        vm_read_script_export_func_param(
            rawBuffer, static_cast<vm_uint32_t>(bytecode.size()), 0, 1, &apiParam, &apiBaseType),
        &apiParam);
    EXPECT_EQ(apiBaseType, 0);
    EXPECT_EQ(
        vm_read_script_export_func_param(
            rawBuffer, static_cast<vm_uint32_t>(bytecode.size()), 1, 0, &apiParam, &apiBaseType),
        &apiParam);
    EXPECT_EQ(apiParam.type, VM_SCRIPT_TYPE_INT);
    EXPECT_EQ(apiBaseType, 0);
    EXPECT_EQ(
        vm_read_script_export_native_param(
            rawBuffer, static_cast<vm_uint32_t>(bytecode.size()), 0, 0, &apiParam, &apiBaseType),
        &apiParam);
    EXPECT_EQ(utils::readString(apiBaseType, data), "B");
    EXPECT_EQ(
        vm_read_script_export_native_param(
            rawBuffer, static_cast<vm_uint32_t>(bytecode.size()), 1, 0, &apiParam, &apiBaseType),
        &apiParam);
    EXPECT_EQ(apiParam.type, VM_SCRIPT_TYPE_BOOL);
    EXPECT_EQ(apiBaseType, 0);
    EXPECT_EQ(vm_read_script_export_struct(rawBuffer, static_cast<vm_uint32_t>(bytecode.size()), 1, &apiStruct),
              &apiStruct);
    EXPECT_EQ(utils::readString(apiStruct.name, data), "B");
    EXPECT_EQ(
        vm_read_script_export_struct_field(
            rawBuffer, static_cast<vm_uint32_t>(bytecode.size()), 1, 0, &apiField, &apiBaseType),
        &apiField);
    EXPECT_EQ(apiField.type, VM_SCRIPT_TYPE_BOOL);
    EXPECT_EQ(apiBaseType, 0);
    EXPECT_EQ(vm_read_script_export_func(rawBuffer, static_cast<vm_uint32_t>(bytecode.size()), 1, &apiFunction),
              &apiFunction);
    EXPECT_EQ(vm_read_script_export_native(rawBuffer, static_cast<vm_uint32_t>(bytecode.size()), 1, &apiNative),
              &apiNative);
    EXPECT_EQ(utils::readString(apiFunction.name, data), "zetaRun");
    EXPECT_EQ(utils::readString(apiNative.name, data), "zetaNative");
}

TEST(Translator, LeavesExternalTypesAndRuntimeSymbolsUnresolved) {
    const std::string source =
        ".header\n"
        "    format: ADVT\n"
        "    version: 1.0.0\n"
        ".export\n"
        "    module test 0\n"
        "    struct Box { value: ImportedType; }\n"
        "    var current: ImportedType\n"
        "    function run(value: ImportedType)\n"
        ".text\n"
        "    push_global run\n"
        "    call current\n"
        "    halt\n"
        ".function run (slots: 1):\n"
        "    ret\n";

    utils::TranslateResult result{};
    const auto bytecode = utils::translateTextToBinary(source, result);

    EXPECT_EQ(result.errorCode, utils::TranslateResult::NoError) << result.message;
    EXPECT_FALSE(bytecode.empty());
}
