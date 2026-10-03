#include <cstring>
#include <vector>

#include <gtest/gtest.h>
#include <utils/translator.hpp>
#include <vm/vm-script.h>

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
