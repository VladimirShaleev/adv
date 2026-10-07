#ifndef ADV_CONTEXT_HPP
#define ADV_CONTEXT_HPP

#include "compiler.hpp"
#include "errors.hpp"
#include "options.hpp"
#include "symbol.hpp"

struct _adv_compilation_result : public utils::Object {};

namespace adv {

class Context : public _adv_compilation_result {
public:
    [[nodiscard]] bool hasNotes() const noexcept { return _hasNotes; }
    [[nodiscard]] bool hasWarnings() const noexcept { return _hasWarnings; }
    [[nodiscard]] bool hasErrors() const noexcept { return _hasErrors; }

    [[nodiscard]] vm_string_h intern(std::string_view str) { return _strPool.insert(str); }
    [[nodiscard]] std::string_view getString(vm_string_h str) const noexcept { return _strPool[str]; }
    [[nodiscard]] std::optional<vm_string_h> findString(std::string_view str) const noexcept {
        return _strPool.find(str);
    }

    [[nodiscard]] SymbolTable& symbolTable() noexcept { return _symbolTable; }

    [[nodiscard]] adv_node_t* allocNode(const adv_location_t& loc, adv_node_type_t type);

    [[nodiscard]] adv_node_t* root(const std::string& module = {}) noexcept;
    [[nodiscard]] vm_string_h rootModule() const noexcept;

    [[nodiscard]] const std::vector<adv_uint8_t>& bytecode(const std::string& module = {}) const noexcept;
    [[nodiscard]] adv_uint16_t rootFrameSlots() const noexcept { return _rootFrameSlots; }

    [[nodiscard]] utils::Format codeFormat(const std::string& module = {}) const noexcept;

    [[nodiscard]] vm_string_h findFilenameByPath(const std::filesystem::path& path) noexcept;

    [[nodiscard]] std::filesystem::path getBasePath(const std::string& module) const noexcept;

    [[nodiscard]] bool hasImport(const std::filesystem::path& path) const noexcept;

    void addImport(const std::filesystem::path& path);

    void setNodeTree(const std::filesystem::path& path, adv_node_t* root);

    void
    setBytecode(const std::filesystem::path& path, std::vector<adv_uint8_t>&& bytecode, utils::Format format) noexcept;

    void setRootFrameSlots(adv_uint16_t slots) noexcept { _rootFrameSlots = slots; }

    template <adv_status_t Status, typename... Args>
    void log(const Options& options, const adv_location_t& loc, Args&&... args) {
        const auto warnAsError = options.getWarningsAsErrors();
        addMessage(Status, warnAsError, loc.filename, loc.line, loc.column, [&]() {
            return err<Status>(unpackArg(std::forward<Args>(args))...);
        });
    }

    virtual void addMessage(
        adv_status_t status,
        bool warnAsError,
        vm_string_h filename,
        adv_uint32_t line,
        adv_uint32_t column,
        std::function<std::string(void)> message) = 0;

    virtual void getMessages(adv_uint32_t& messageCount, adv_message_t* messages) const noexcept { messageCount = 0; }

    virtual void clearMessages() noexcept {
        _hasNotes = 0;
        _hasWarnings = 0;
        _hasErrors = 0;
    };

protected:
    bool applyStatusAndCheckIsError(adv_status_t status, bool warnAsError) noexcept;

private:
    static constexpr size_t MaxNodesInBucket = 1024;

    template <typename T>
    T&& unpackArg(T&& arg) const noexcept {
        return static_cast<T&&>(arg);
    }
    std::string_view unpackArg(vm_string_h str) const noexcept { return getString(str); }
    std::string unpackArg(adv_location_t loc) const {
        return std::format("{}:{}:{}", getString(loc.filename), loc.line, loc.column);
    }

    struct Import {
        adv_node_t* root{};
        std::filesystem::path filename{};
        std::filesystem::path basePath{};
        std::vector<adv_uint8_t> bytecode{};
        utils::Format format{};
    };

    bool _hasNotes{};
    bool _hasWarnings{};
    bool _hasErrors{};
    adv_uint16_t _rootFrameSlots{};
    utils::StringPool _strPool{};
    std::vector<std::vector<adv_node_t>> _nodes{};
    size_t _nodeCount{};
    std::unordered_map<std::string, Import> _imports{};
    adv_node_t* _main{};
    vm_string_h _mainModule{};
    SymbolTable _symbolTable{};
};

class ContextImpl final : public Context {
public:
    void addMessage(
        adv_status_t status,
        bool warnAsError,
        vm_string_h filename,
        adv_uint32_t line,
        adv_uint32_t column,
        std::function<std::string(void)> message) override;

    void getMessages(adv_uint32_t& messageCount, adv_message_t* messages) const noexcept override;

    void clearMessages() noexcept override;

private:
    std::vector<adv_message_t> _messages{};
};

class ContextStub final : public Context {
public:
    void addMessage(
        adv_status_t status,
        bool warnAsError,
        vm_string_h filename,
        adv_uint32_t line,
        adv_uint32_t column,
        std::function<std::string(void)> message) override;
};

} // namespace adv

#endif // ADV_CONTEXT_HPP
