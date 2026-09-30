#include <iostream>

#include <adv/adv.h>
#include <argparse/argparse.hpp>

using namespace std::literals;

int main(int argc, char* argv[]) {
    auto warnAsErr = false;
    auto input = std::filesystem::path();
    auto output = std::filesystem::current_path();
    auto imports = std::vector<std::string>();
    argparse::ArgumentParser program("advc", "1.0.0");

    program.add_argument("input").store_into(input).help("input .adv file");
    program.add_argument("-o", "--output").store_into(output).help("output path (directory or file)");
    program.add_argument("-i", "--imports").append().store_into(imports).help("import directories");
    program.add_argument("-w", "--warnings").store_into(warnAsErr).help("warnings as errors");

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& exc) {
        std::cerr << exc.what() << std::endl;
        std::cerr << program;
        return EXIT_FAILURE;
    }

    const auto inputFile = input.string();
    const auto outputPath = output.string();
    std::vector<adv_utf8_t> dirs;
    for (const auto& import : imports) {
        dirs.push_back(import.c_str());
    }

    adv_options_t options{};
    auto code = adv_options_create(&options);
    if (code != ADV_RESULT_SUCCESS) {
        std::cerr << adv_result_to_string(code) << std::endl;
        return EXIT_FAILURE;
    }
    adv_options_set_warnings_as_errors(options, warnAsErr ? 1 : 0);
    adv_options_set_output_path(options, outputPath.c_str());
    adv_options_set_import_dirs(options, (adv_uint32_t)dirs.size(), dirs.data());

    adv_compiler_t compiler{};
    adv_compiler_create(&compiler);
    if (code != ADV_RESULT_SUCCESS) {
        adv_options_destroy(options);
        std::cerr << adv_result_to_string(code) << std::endl;
        return EXIT_FAILURE;
    }

    adv_compilation_result_t result{};
    code = adv_compiler_compile(compiler, inputFile.c_str(), 0, nullptr, options, &result);

    bool failed = false;
    if (result) {
        const auto hasNotes = adv_compilation_result_has_notes(result);
        const auto hasWarns = adv_compilation_result_has_warnings(result);
        const auto hasErrors = adv_compilation_result_has_errors(result);

        if (hasErrors) {
            failed = true;
        }

        if (hasNotes || hasWarns || hasErrors) {
            adv_uint32_t count{};
            adv_compilation_result_get_messages(result, &count, nullptr);

            std::vector<adv_message_t> messages;
            messages.resize(count);
            adv_compilation_result_get_messages(result, &count, messages.data());

            for (const auto& message : messages) {
                auto messageType = "<unknown>"sv;
                auto messagePrefix = 'U';
                if (message.is_error) {
                    messageType = "error"sv;
                } else if (message.status >= ADV_STATUS_W2001) {
                    messageType = "warning"sv;
                } else {
                    messageType = "note"sv;
                }
                if (message.status >= ADV_STATUS_E3001) {
                    messagePrefix = 'E';
                } else if (message.status >= ADV_STATUS_W2001) {
                    messagePrefix = 'W';
                } else {
                    messagePrefix = 'N';
                };

                std::cerr << messageType << " [" << messagePrefix << (int)message.status << "]: " << message.message;
                if (message.line > 0) {
                    std::cerr << " at " << message.filename << ':' << message.line << ':' << message.column;
                }
                std::cerr << '.' << std::endl;
            }
        }
    }
    if (code != ADV_RESULT_SUCCESS && !failed) {
        std::cerr << "error: " << adv_result_to_string(code) << std::endl;
        failed = true;
    }

    adv_compilation_result_destroy(result);
    adv_compiler_destroy(compiler);
    adv_options_destroy(options);

    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
