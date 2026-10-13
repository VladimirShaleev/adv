#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <utils/translator.hpp>
#include <vm/vm-script.h>
#include <vm/vm-value.h>

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
        "    function myfunc(a: int, b: float): float 3\n"
        ".text\n"
        "myfunc:\n"
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
    EXPECT_EQ(function.frame_slot_count, 3);
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
