#include "context.hpp"
#include "visitor.hpp"

using namespace std::literals;

namespace adv {

adv_node_t* Context::allocNode(const adv_location_t& loc, adv_node_type_t type) {
    auto index = _nodeCount++;
    if (index % MaxNodesInBucket == 0) {
        _nodes.push_back({});
        _nodes.back().resize(MaxNodesInBucket);
    }
    auto& backet = _nodes.back();
    auto& node = backet[index % MaxNodesInBucket];

    node.location = loc;
    node.type = type;
    return &node;
}

adv_node_t* Context::root(const std::string& module) noexcept {
    if (!module.empty()) {
        if (auto it = _imports.find(module); it != _imports.end()) {
            return it->second.root;
        }
        return nullptr;
    }
    return _main;
}

vm_string_h Context::findFilenameByPath(const std::filesystem::path& path) const noexcept {
    for (const auto& [module, import] : _imports) {
        try {
            if (std::filesystem::equivalent(import.filename, path)) {
                const auto str = path.string();
                return this->findString({str.c_str(), str.length()}).value_or({0});
            }
        } catch (const std::filesystem::filesystem_error&) {
        }
    }
    return {0};
}

std::filesystem::path Context::getBasePath(const std::string& module) const noexcept {
    if (auto it = _imports.find(module); it != _imports.end()) {
        return it->second.basePath;
    }
    return std::filesystem::current_path();
}

bool Context::hasImport(const std::filesystem::path& path) const noexcept {
    const auto module = path.filename().replace_extension().string();
    return _imports.contains(module);
}

void Context::addImport(const std::filesystem::path& path) {
    const auto module = path.filename().replace_extension().string();
    auto& import = _imports[module];
    import.filename = path;
    import.basePath = path.parent_path();
}

void Context::setNodeTree(const std::filesystem::path& path, adv_node_t* root) {
    const auto module = path.filename().replace_extension().string();
    if (auto it = _imports.find(module); it != _imports.end()) {
        it->second.root = root;
    } else {
        assert(!"unreachable code");
    }
    if (!_main) {
        _main = root;
    }
}

bool Context::applyStatusAndCheckIsError(adv_status_t status, bool warnAsError) noexcept {
    const auto startError = warnAsError ? ADV_STATUS_W2001 : ADV_STATUS_E3001;
    if (status >= startError) {
        _hasErrors = true;
        return true;
    } else if (status >= ADV_STATUS_W2001) {
        _hasWarnings = true;
    } else {
        _hasNotes = true;
    }
    return false;
}

void ContextImpl::addMessage(
    adv_status_t status,
    bool warnAsError,
    vm_string_h filename,
    adv_uint32_t line,
    adv_uint32_t column,
    std::function<std::string(void)> message) {
    const auto msgStr = message();
    const auto msgHandle = intern({msgStr.c_str(), msgStr.length()});
    _messages.emplace_back(
        status,
        applyStatusAndCheckIsError(status, warnAsError),
        getString(msgHandle).data(),
        getString(filename).data(),
        line,
        column);
}

void ContextImpl::getMessages(adv_uint32_t& messageCount, adv_message_t* messages) const noexcept {
    if (messages) {
        messageCount = std::min(messageCount, (adv_uint32_t)_messages.size());
        std::copy(_messages.cbegin(), _messages.cend(), messages);
    } else {
        messageCount = (adv_uint32_t)_messages.size();
    }
}

void ContextImpl::clearMessages() noexcept {
    Context::clearMessages();
    _messages.clear();
}

void ContextStub::addMessage(
    adv_status_t status,
    bool warnAsError,
    vm_string_h /* filename */,
    adv_uint32_t /* line */,
    adv_uint32_t /* column */,
    std::function<std::string(void)> /* message */) {
    applyStatusAndCheckIsError(status, warnAsError);
}

} // namespace adv

adv_utf8_t adv_result_to_string(adv_result_t result) {
    switch (result) {
    case ADV_RESULT_SUCCESS:
        return "no error has occurred";
    case ADV_RESULT_COMPILATION_FAILED:
        return "compilation failed";
    case ADV_RESULT_ERROR_UNKNOWN:
        return "unknown error";
    case ADV_RESULT_ERROR_OUT_OF_MEMORY:
        return "out of memory";
    case ADV_RESULT_ERROR_INVALID_ARG:
        return "invalid argument";
    case ADV_RESULT_ERROR_FILE_CREATE:
        return "failed to create file";
    case ADV_RESULT_ERROR_SOURCE_NOT_FOUND:
        return "source not found";
    default:
        return "<unknown result>";
    }
}

adv_compilation_result_t adv_compilation_result_reference(adv_compilation_result_t compilation_result) {
    if (compilation_result) {
        compilation_result->as<adv::Context>()->reference();
    }
    return compilation_result;
}

void adv_compilation_result_destroy(adv_compilation_result_t compilation_result) {
    if (compilation_result) {
        compilation_result->as<adv::Context>()->destroy();
    }
}

adv_bool_t adv_compilation_result_has_notes(adv_compilation_result_t compilation_result) {
    return compilation_result ? compilation_result->as<adv::Context>()->hasNotes() : 0;
}

adv_bool_t adv_compilation_result_has_warnings(adv_compilation_result_t compilation_result) {
    return compilation_result ? compilation_result->as<adv::Context>()->hasWarnings() : 0;
}

adv_bool_t adv_compilation_result_has_errors(adv_compilation_result_t compilation_result) {
    return compilation_result ? compilation_result->as<adv::Context>()->hasErrors() : 0;
}

void adv_compilation_result_get_messages(
    adv_compilation_result_t compilation_result, adv_uint32_t* message_count, adv_message_t* messages) {
    if (message_count) {
        if (compilation_result) {
            compilation_result->as<adv::Context>()->getMessages(*message_count, messages);
        } else {
            *message_count = 0;
        }
    }
}

const adv_node_t* adv_compilation_result_get_root_node(adv_compilation_result_t compilation_result) {
    return compilation_result ? compilation_result->as<adv::Context>()->root() : nullptr;
}

adv_scope_h adv_compilation_result_get_global_scope(adv_compilation_result_t compilation_result) {
    return compilation_result ? compilation_result->as<adv::Context>()->symbolTable().getGlobalScope()
                              : adv::Scope::Undefined;
}

adv_symbol_h
adv_compilation_result_lookup_symbol(adv_compilation_result_t compilation_result, adv_scope_h scope, adv_utf8_t name) {
    if (compilation_result && scope != adv::Scope::Undefined && name) {
        if (auto str = compilation_result->as<adv::Context>()->findString(name)) {
            return compilation_result->as<adv::Context>()->symbolTable().lookup(scope, str.value());
        }
    }
    return adv::Symbol::Undefined;
}

void adv_compilation_result_get_visible_symbols_at(
    adv_compilation_result_t compilation_result,
    adv_utf8_t filename,
    adv_uint32_t line,
    adv_uint32_t column,
    adv_symbol_h* symbols,
    adv_uint32_t* symbol_count) {
    if (!compilation_result || !filename || !symbol_count || (!symbol_count && !symbols)) {
        if (symbol_count) {
            *symbol_count = 0;
        }
        return;
    }
    auto path = compilation_result->as<adv::Context>()->findFilenameByPath(filename);
    if (path == vm_string_h{0}) {
        if (symbol_count) {
            *symbol_count = 0;
        }
        return;
    }
    const adv_location_t location{path, line, column};
    compilation_result->as<adv::Context>()->symbolTable().getVisibleSymbolsAt(location, symbols, *symbol_count);
}

const adv_scope_t* adv_compilation_result_get_scope(adv_compilation_result_t compilation_result, adv_scope_h scope) {
    return compilation_result ? compilation_result->as<adv::Context>()->symbolTable().getRawScope(scope) : nullptr;
}

const adv_symbol_t*
adv_compilation_result_get_symbol(adv_compilation_result_t compilation_result, adv_symbol_h symbol) {
    return compilation_result ? compilation_result->as<adv::Context>()->symbolTable().getRawSymbol(symbol) : nullptr;
}

adv_utf8_t adv_compilation_result_get_string(
    adv_compilation_result_t compilation_result, vm_string_h string, adv_uint32_t* length) {
    if (compilation_result) {
        const auto view = compilation_result->as<adv::Context>()->getString(string);
        if (length) {
            *length = static_cast<adv_uint32_t>(view.length());
        }
        return view.data();
    }
    if (length) {
        *length = 0;
    }
    return "";
}
