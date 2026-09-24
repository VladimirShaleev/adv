#include "analyzer.hpp"
#include "parser.hpp"
#include "scanner.hpp"

using namespace std::literals;

struct _adv_compiler : public utils::Object {};

namespace adv {

class Compiler final : public _adv_compiler {
public:
    adv_result_t compile(
        adv_utf8_t file, std::span<const adv_source_t> sources, const Options& options, ContextBase& context) noexcept {
        Analyzer analyzer(options, context);
        adv_result_t result = ADV_RESULT_SUCCESS;
        std::stack<std::tuple<std::string, adv_node_t*, std::filesystem::path, adv_uint32_t>> stack;
        stack.emplace(""s, nullptr, file ? file : "", 0);

        while (!stack.empty()) {
            const auto [parentModule, parentImport, currFile, depth] = stack.top();
            stack.pop();

            const auto [status, module] = generateNodeTree(parentModule, currFile, sources, depth, options, context);
            if (status != ADV_RESULT_SUCCESS && result == ADV_RESULT_SUCCESS) {
                result = status;
            }
            if (status == ADV_RESULT_SUCCESS) {
                auto moduleRoot = context.root(module);
                if (parentImport) {
                    parentImport->import_stmt.root = moduleRoot;
                }
                auto curr = moduleRoot->children.first_child;
                while (curr) {
                    if (curr->type == ADV_NODE_IMPORT_STMT) {
                        auto import = context.getString(curr->import_stmt.path);
                        if (import.empty()) {
                            // Log error
                        } else {
                            stack.emplace(module, curr, import, depth + 1);
                        }
                    }
                    curr = curr->next_sibling;
                }
            }
        }

        if (result != ADV_RESULT_SUCCESS) {
            return result;
        }

        return analyzer.build();
    }

private:
    std::pair<adv_result_t, std::string> generateNodeTree(
        const std::string& parentModule,
        std::filesystem::path file,
        std::span<const adv_source_t> sources,
        adv_uint32_t depth,
        const Options& options,
        ContextBase& context) {

        Scanner scanner{context, options, sources, file, context.getBasePath(parentModule), depth};
        Parser parser{scanner};

        // parser.set_debug_level(1);

        if (scanner.filename() == nullptr) {
            // const auto warnAsErrors = options ? options->getWarningsAsErrors() : false;
            // const auto sourceHandle = result->intern("<input>");
            // result->addMessage(
            //     IDL_STATUS_E3046, warnAsErrors, sourceHandle, 0, 0, []() { return err<IDL_STATUS_E3046>(); });
            return {ADV_RESULT_ERROR_SOURCE_NOT_FOUND, ""};
        }

        const auto module = std::filesystem::path(*scanner.filename()).filename().replace_extension().string();

        const auto code = parser.parse();

        if (context.hasErrors() || code != 0) {
            if (!context.hasErrors()) {
                // const auto warnAsErrors = options ? options->getWarningsAsErrors() : false;
                // const auto sourceHandle = result->intern("<input>");
                // result->addMessage(
                //     IDL_STATUS_E3046, warnAsErrors, sourceHandle, 0, 0, []() { return err<IDL_STATUS_E3046>(); });
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
    adv::ContextBase* context{};
    adv::Options& ops = options ? *options->as<adv::Options>() : optionsStub;
    if (result) {
        if (auto code = utils::Object::create<adv_result_t, adv::Context>(*result); code != ADV_RESULT_SUCCESS) {
            return code;
        }
        context = (*result)->as<adv::ContextBase>();
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
