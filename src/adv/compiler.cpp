#include "analyzer.hpp"
#include "generator.hpp"
#include "parser.hpp"
#include "scanner.hpp"

using namespace std::literals;

struct _adv_compiler : public utils::Object {};

namespace adv {

class Compiler final : public _adv_compiler {
public:
    adv_result_t
    compile(adv_utf8_t file, std::span<const adv_source_t> sources, const Options& options, Context& context) noexcept {
        Analyzer analyzer(options, context);
        Generator generator(options, context);
        std::pair<adv_result_t, std::string> parseResult{};
        if (parseResult = parse(file, sources, options, context); parseResult.first != ADV_RESULT_SUCCESS) {
            return parseResult.first;
        }
        if (auto result = analyzer.build(); result != ADV_RESULT_SUCCESS) {
            return result;
        }
        if (auto result = generator.gen(); result != ADV_RESULT_SUCCESS) {
            return result;
        }
        if (auto result = generator.text(); result != ADV_RESULT_SUCCESS) {
            return result;
        }
        return write(options, context, parseResult.second);
    }

private:
    adv_result_t write(const Options& options, Context& context, const std::string& module) {
        const auto& code = context.bytecode();
        adv_data_t data{};
        if (auto writer = options.getWriter(&data)) {
            adv_source_t source{module.c_str(), (const adv_char_t*)code.data(), static_cast<adv_uint32_t>(code.size())};
            writer(&source, data);
        } else {
            auto cDir = options.getOutputDir();
            auto outputPath = cDir && cDir[0] != '\0' ? std::filesystem::path(cDir) : std::filesystem::current_path();
            std::error_code error{};
            const auto isDirectory = std::filesystem::is_directory(outputPath, error);
            if (error && std::filesystem::exists(outputPath)) {
                return ADV_RESULT_ERROR_FILE_CREATE;
            }
            if (isDirectory) {
                std::filesystem::create_directories(outputPath, error);
                if (error) {
                    return ADV_RESULT_ERROR_FILE_CREATE;
                }
                outputPath /= module;
            }
            if (outputPath.extension().empty()) {
                switch (context.codeFormat()) {
                case utils::Format::Advb:
                    outputPath += ".adb"sv;
                    break;
                case utils::Format::Advt:
                    outputPath += ".adt"sv;
                    break;
                default:
                    assert(!"unreachable code");
                    break;
                }
            }
            std::ofstream output(outputPath, std::ios::binary);
            if (!output) {
                return ADV_RESULT_ERROR_FILE_CREATE;
            }
            output.write(reinterpret_cast<const char*>(code.data()), static_cast<std::streamsize>(code.size()));
            if (!output) {
                return ADV_RESULT_ERROR_FILE_CREATE;
            }
        }
        return ADV_RESULT_SUCCESS;
    }

    [[nodiscard]] std::pair<adv_result_t, std::string>
    parse(adv_utf8_t file, std::span<const adv_source_t> sources, const Options& options, Context& context) {
        adv_result_t result = ADV_RESULT_SUCCESS;
        std::string mainModule;
        std::stack<std::tuple<std::string, adv_node_t*, std::filesystem::path, adv_uint32_t>> stack;
        stack.emplace(""s, nullptr, file ? file : "", 0);

        while (!stack.empty()) {
            const auto [parentModule, parentImport, currFile, depth] = stack.top();
            stack.pop();

            const auto parentLoc = parentImport ? parentImport->location : adv_location_t{};
            const auto [status, module] =
                generateNodeTree(parentModule, parentLoc, currFile, sources, depth, options, context);
            if (status != ADV_RESULT_SUCCESS && result == ADV_RESULT_SUCCESS) {
                result = status;
            }
            if (status == ADV_RESULT_SUCCESS) {
                if (mainModule.empty()) {
                    mainModule = module;
                }
                if (!context.bytecode().empty()) {
                    return {ADV_RESULT_SUCCESS, mainModule};
                }
                auto moduleRoot = context.root(module);
                if (parentImport) {
                    parentImport->import_stmt.root = moduleRoot;
                }
                auto curr = moduleRoot->stmts.first_stmt;
                while (curr) {
                    if (curr->type == ADV_NODE_IMPORT_STMT) {
                        auto import = context.getString(curr->import_stmt.path);
                        if (import.empty()) {
                            context.log<ADV_STATUS_E3039>(options, curr->location);
                        } else {
                            stack.emplace(module, curr, import, depth + 1);
                        }
                    }
                    curr = curr->next_sibling;
                }
            }
        }
        return {result, mainModule};
    }

    [[nodiscard]] std::pair<adv_result_t, std::string> generateNodeTree(
        const std::string& parentModule,
        const adv_location_t& parentLoc,
        std::filesystem::path file,
        std::span<const adv_source_t> sources,
        adv_uint32_t depth,
        const Options& options,
        Context& context) {

        Scanner scanner{context, options, sources, parentLoc, file, context.getBasePath(parentModule), depth};
        Parser parser{scanner};

        if (scanner.filename() == nullptr) {
            return {ADV_RESULT_ERROR_SOURCE_NOT_FOUND, ""};
        }

        if (depth == 0 && !context.bytecode().empty()) {
            return {ADV_RESULT_SUCCESS, ""};
        }

        const auto module = std::filesystem::path(*scanner.filename()).filename().replace_extension().string();

        const auto code = parser.parse();

        if (context.hasErrors() || code != 0) {
            if (depth == 0 && scanner.parseAdvt() && !context.bytecode().empty()) {
                return {ADV_RESULT_SUCCESS, ""};
            }
            if (!context.hasErrors()) {
                context.log<ADV_STATUS_E3002>(options, {});
            }
            return {ADV_RESULT_COMPILATION_FAILED, module};
        }
        return {ADV_RESULT_SUCCESS, module};
    }
};

} // namespace adv

adv_result_t adv_compiler_create(adv_compiler_t* compiler) {
    return compiler ? utils::Object::create<adv_result_t, adv::Compiler>(*compiler) : ADV_RESULT_ERROR_INVALID_ARG;
}

adv_compiler_t adv_compiler_reference(adv_compiler_t compiler) {
    if (compiler) {
        compiler->as<adv::Compiler>()->reference();
    }
    return compiler;
}

void adv_compiler_destroy(adv_compiler_t compiler) {
    if (compiler) {
        compiler->as<adv::Compiler>()->destroy();
    }
}

adv_result_t adv_compiler_compile(
    adv_compiler_t compiler,
    adv_utf8_t file,
    adv_uint32_t source_count,
    const adv_source_t* sources,
    adv_options_t options,
    adv_compilation_result_t* result) {
    adv::Options optionsStub{};
    adv::Context* context{};
    adv::Options& ops = options ? *options->as<adv::Options>() : optionsStub;
    if (result) {
        if (auto code = utils::Object::create<adv_result_t, adv::ContextImpl>(*result); code != ADV_RESULT_SUCCESS) {
            return code;
        }
        context = (*result)->as<adv::Context>();
        context->reference();
    } else {
        if (auto code = utils::Object::create<adv_result_t, adv::ContextStub>(context); code != ADV_RESULT_SUCCESS) {
            return code;
        }
    }
    assert(context);
    const auto code =
        compiler
            ? compiler->as<adv::Compiler>()->compile(file, {sources, source_count}, std::cref(ops), std::ref(*context))
            : ADV_RESULT_ERROR_INVALID_ARG;
    context->destroy();
    return code;
}
