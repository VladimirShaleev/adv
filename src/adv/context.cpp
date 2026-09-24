#include "context.hpp"
#include "visitor.hpp"

using namespace std::literals;

namespace adv {

adv_node_t* ContextBase::allocNode(const adv_location_t& loc, adv_node_type_t type) {
    auto index = _nodeCount++;
    if (index % MaxNodesInBucket == 0) {
        _nodes.push_back({});
        _nodes.back().resize(MaxNodesInBucket);
    }
    auto& backet = _nodes.back();
    auto& node   = backet[index % MaxNodesInBucket];

    node.location = loc;
    node.type     = type;
    return &node;
}

[[nodiscard]] adv_node_t* ContextBase::root(const std::string& module) noexcept {
    if (!module.empty()) {
        if (auto it = _imports.find(module); it != _imports.end()) {
            return it->second.root;
        }
        return nullptr;
    }
    return _main;
}

std::filesystem::path ContextBase::getBasePath(const std::string& module) const noexcept {
    if (auto it = _imports.find(module); it != _imports.end()) {
        return it->second.basePath;
    }
    return std::filesystem::current_path();
}

bool ContextBase::hasImport(const std::filesystem::path& path) const noexcept {
    const auto module = path.filename().replace_extension().string();
    return _imports.contains(module);
}

void ContextBase::addImport(const std::filesystem::path& path) {
    const auto module = path.filename().replace_extension().string();
    auto& import      = _imports[module];
    import.filename   = path;
    import.basePath   = path.parent_path();
}

void ContextBase::setNodeTree(const std::filesystem::path& path, adv_node_t* root) {
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

bool ContextBase::applyStatusAndCheckIsError(adv_status_t status, bool warnAsError) noexcept {
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

void Context::addMessage(
    adv_status_t status,
    bool warnAsError,
    vm_string_h filename,
    adv_uint32_t line,
    adv_uint32_t column,
    std::function<std::string(void)> message) {
    const auto msgStr    = message();
    const auto msgHandle = intern({msgStr.c_str(), msgStr.length()});
    _messages.emplace_back(
        status,
        applyStatusAndCheckIsError(status, warnAsError),
        getString(msgHandle).data(),
        getString(filename).data(),
        line,
        column);
}

void Context::getMessages(adv_uint32_t& messageCount, adv_message_t* messages) const noexcept {
    if (messages) {
        messageCount = std::min(messageCount, (adv_uint32_t)_messages.size());
        std::copy(_messages.cbegin(), _messages.cend(), messages);
    } else {
        messageCount = (adv_uint32_t)_messages.size();
    }
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
        compilation_result->as<adv::ContextBase>()->reference();
    }
    return compilation_result;
}

void adv_compilation_result_destroy(adv_compilation_result_t compilation_result) {
    if (compilation_result) {
        compilation_result->as<adv::ContextBase>()->destroy();
    }
}

adv_bool_t adv_compilation_result_has_notes(adv_compilation_result_t compilation_result) {
    return compilation_result ? compilation_result->as<adv::ContextBase>()->hasNotes() : 0;
}

adv_bool_t adv_compilation_result_has_warnings(adv_compilation_result_t compilation_result) {
    return compilation_result ? compilation_result->as<adv::ContextBase>()->hasWarnings() : 0;
}

adv_bool_t adv_compilation_result_has_errors(adv_compilation_result_t compilation_result) {
    return compilation_result ? compilation_result->as<adv::ContextBase>()->hasErrors() : 0;
}

void adv_compilation_result_get_messages(
    adv_compilation_result_t compilation_result, adv_uint32_t* message_count, adv_message_t* messages) {
    if (message_count) {
        if (compilation_result) {
            compilation_result->as<adv::ContextBase>()->getMessages(*message_count, messages);
        } else {
            *message_count = 0;
        }
    }
}

const adv_node_t* adv_compilation_result_get_root_node(adv_compilation_result_t compilation_result) {
    return compilation_result ? compilation_result->as<adv::ContextBase>()->root() : nullptr;
}

adv_utf8_t adv_compilation_result_get_string(adv_compilation_result_t compilation_result, vm_string_h string) {
    return compilation_result ? compilation_result->as<adv::ContextBase>()->getString(string).data() : nullptr;
}
