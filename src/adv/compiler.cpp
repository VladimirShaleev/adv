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
        if (auto result = parse(file, sources, options, context); result != ADV_RESULT_SUCCESS) {
            return result;
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
        return write(options, context);
    }

private:
    adv_result_t write(const Options& options, Context& context) {
        const auto module = context.getString(context.rootModule());
        const auto& code = context.bytecode();
        adv_data_t data{};
        if (auto writer = options.getWriter(&data)) {
            adv_source_t source{module.data(), (const adv_char_t*)code.data(), static_cast<adv_uint32_t>(code.size())};
            writer(&source, data);
        }
#if !defined(__EMSCRIPTEN__)
        else {
            auto cPath = options.getOutputPath();
            auto outputPath =
                cPath && cPath[0] != '\0' ? std::filesystem::path(cPath) : std::filesystem::current_path();
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
            output.write((const char*)code.data(), static_cast<std::streamsize>(code.size()));
            if (!output) {
                return ADV_RESULT_ERROR_FILE_CREATE;
            }
        }
#endif
        return ADV_RESULT_SUCCESS;
    }

    [[nodiscard]] adv_result_t
    parse(adv_utf8_t file, std::span<const adv_source_t> sources, const Options& options, Context& context) {
        adv_result_t result = ADV_RESULT_SUCCESS;
        std::stack<std::tuple<std::string, adv_node_t*, std::filesystem::path, adv_uint32_t>> stack;
        stack.emplace(""s, nullptr, file ? file : "", 0);

        while (!stack.empty()) {
            const auto [parentModule, parentImport, currFile, depth] = stack.top();
            stack.pop();
            if (context.hasImport(currFile)) {
                if (parentImport) {
                    parentImport->import_stmt.root = context.root(utils::getModuleName(currFile));
                }
                continue;
            }
            const auto parentLoc = parentImport ? parentImport->location : adv_location_t{};
            const auto [status, module] =
                generateNodeTree(parentModule, parentLoc, currFile, sources, depth, options, context);
            if (status != ADV_RESULT_SUCCESS && result == ADV_RESULT_SUCCESS) {
                result = status;
            }
            if (status == ADV_RESULT_SUCCESS) {
                if (auto bytecode = context.bytecode(module); !bytecode.empty()) {
                    getImportsFromBinary(context, bytecode);
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
        return result;
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

        const auto moduleName = utils::getModuleName(*scanner.filename());
        if (!context.bytecode(moduleName).empty()) {
            return {ADV_RESULT_SUCCESS, moduleName};
        }

        const auto code = parser.parse();
        if (context.hasErrors() || code != 0) {
            if (scanner.parseAdvt() && !context.bytecode(moduleName).empty()) {
                return {ADV_RESULT_SUCCESS, moduleName};
            }
            if (!context.hasErrors()) {
                context.log<ADV_STATUS_E3002>(options, {});
            }
            return {ADV_RESULT_COMPILATION_FAILED, moduleName};
        }
        return {ADV_RESULT_SUCCESS, moduleName};
    }

    void getImportsFromBinary(Context& ctx, std::span<const uint8_t> bytecode) {
        auto moduleName = utils::read<vm_script_export_t>(bytecode).module_name;
        auto data = utils::read<utils::SectionData>(bytecode).data;
        auto text = utils::read<utils::SectionText>(bytecode).text;
        adv_location_t loc{ctx.intern(utils::readString(moduleName, data)), 0, 0};
        auto program = ctx.allocNode(loc, ADV_NODE_PROGRAM);
        adv_node_t* prevImport = nullptr;
        ctx.setNodeTree(utils::readString(moduleName, data), program);
        if (!text.empty()) {
            vm_uint32_t ip = 0;
            while (ip < text.size()) {
                const auto opcode = (vm_opcode_t)text.data()[ip++];
                switch (opcode) {
                case VM_OPCODE_IMPORT:
                    if (auto importName = utils::readString(text, ip, data); !importName.empty()) {
                        auto import = ctx.allocNode(loc, ADV_NODE_IMPORT_STMT);
                        import->import_stmt.path = ctx.intern(importName);
                        if (program->stmts.first_stmt == nullptr) {
                            program->stmts.first_stmt = import;
                            prevImport = import;
                        } else {
                            prevImport->next_sibling = import;
                            prevImport = import;
                        }
                        ++program->stmts.stmt_count;
                    }
                    break;
                case VM_OPCODE_PUSH_INT:
                case VM_OPCODE_PUSH_FLOAT:
                case VM_OPCODE_PUSH_STRING_FROM_DATA:
                case VM_OPCODE_PUSH_GLOBAL_BY_NAME:
                case VM_OPCODE_POP_GLOBAL_BY_NAME:
                case VM_OPCODE_PUSH_GLOBAL:
                case VM_OPCODE_POP_GLOBAL:
                case VM_OPCODE_CALL_NATIVE_BY_NAME:
                case VM_OPCODE_CALL_NATIVE:
                case VM_OPCODE_CALL_BY_NAME:
                case VM_OPCODE_CALL:
                case VM_OPCODE_JUMP:
                case VM_OPCODE_JUMP_IF_FALSE:
                    ip += 4;
                    break;
                case VM_OPCODE_PUSH_LOCAL:
                case VM_OPCODE_POP_LOCAL:
                    ip += 2;
                    break;
                default:
                    break;
                }
            }
        }
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
