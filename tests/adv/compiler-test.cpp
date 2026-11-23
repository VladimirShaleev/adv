#include <adv/adv.h>

#include <gtest/gtest.h>

#include <string>

namespace {

void captureOutput(const adv_source_t* source, adv_data_t data) {
    auto& output = *static_cast<std::string*>(data);
    output.assign(source->data, source->size);
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

} // namespace
