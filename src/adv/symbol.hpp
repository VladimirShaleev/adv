#ifndef ADV_SYMBOL_HPP
#define ADV_SYMBOL_HPP

#include "compiler.hpp"

namespace adv {

enum class SlotLayoutState {
    Unresolved,
    Resolving,
    Resolved,
    Invalid
};

struct Symbol : adv_symbol_t {
    static constexpr adv_symbol_h Undefined = {0};
    std::vector<adv_symbol_h> linearChildren;
    SlotLayoutState slotLayoutState{SlotLayoutState::Unresolved};
};

struct Scope : adv_scope_t {
    static constexpr adv_scope_h Undefined = {0};
    std::vector<adv_scope_h> linearChildren{};
    std::vector<adv_symbol_h> linearSymbols{};
    std::unordered_map<vm_string_h, adv_symbol_h> symbolMap{};
};

class Options;
class Context;

class SymbolTable {
public:
    void init(const Options& options, Context& ctx);
    void beginScope();
    void endScope();
    [[nodiscard]] adv_scope_t* getRawScope(adv_scope_h handle) noexcept;
    [[nodiscard]] adv_symbol_t* getRawSymbol(adv_symbol_h handle) noexcept;
    [[nodiscard]] adv_scope_h getGlobalScope() const noexcept;
    [[nodiscard]] Scope* getScope(adv_scope_h handle) noexcept;
    [[nodiscard]] Symbol* getSymbol(adv_symbol_h handle) noexcept;
    [[nodiscard]] adv_symbol_h lookupVoid() noexcept;
    [[nodiscard]] adv_symbol_h lookup(vm_value_type_t primitiveType) noexcept;
    [[nodiscard]] adv_symbol_h lookup(adv_scope_h scope, vm_string_h name) noexcept;
    [[nodiscard]] std::pair<adv_symbol_h, Symbol*>
    lookup(const Options& options, Context& ctx, const adv_location_t& loc, vm_string_h name) noexcept;
    Symbol* addSymbol(const Options& options, Context& ctx, adv_node_t* node);
    void getVisibleSymbolsAt(const adv_location_t& loc, adv_symbol_h* symbols, adv_uint32_t& symbolCount) noexcept;

private:
    [[nodiscard]] adv_scope_h allocScope(adv_scope_h parent);
    [[nodiscard]] adv_symbol_h allocSymbol(adv_node_t* node);
    [[nodiscard]] static std::string_view getTypeString(vm_value_type_t type) noexcept;

    adv_scope_h _glocalScope{};
    adv_scope_h _scope{};
    std::vector<Scope> _scopePool;
    std::vector<Symbol> _symbolPool;
    std::array<adv_symbol_h, VM_VALUE_TYPE_COUNT_ENUMS> _primitives;
    adv_symbol_h _void;
};

} // namespace adv

#endif // ADV_SYMBOL_HPP
