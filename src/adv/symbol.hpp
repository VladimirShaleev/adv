#ifndef ADV_SYMBOL_HPP
#define ADV_SYMBOL_HPP

#include "compiler.hpp"

namespace adv {

enum class SymbolKind {
    Var,
    Param,
    Function,
    Struct
};

struct Symbol {
    vm_string_h name;
    SymbolKind kind;
    vm_value_type_t type;
    std::vector<Symbol*> params;
    adv_location_t location;
    adv_uint16_t slotIndex;
    adv_uint16_t frameSlotCount;
    bool isGlobal;
};

struct Scope {
    Scope* parent{};
    std::unordered_map<vm_string_h, Symbol*> symbols{};
};

class Options;
class Context;

class SymbolTable {
public:
    SymbolTable();

    void beginScope();
    void endScope();
    void setScope(void* scope);
    Symbol* addSymbol(const Options& options, Context& ctx, adv_node_t* node);
    [[nodiscard]] Symbol*
    lookup(const Options& options, Context& ctx, const adv_location_t& loc, vm_string_h name) const noexcept;
    [[nodiscard]] void* getScope() noexcept;

private:
    Scope* allocScope(Scope* parentScope);
    Symbol* allocSym(adv_node_t* node);

    Scope* _glocalScope{};
    Scope* _scope{};
    std::vector<std::unique_ptr<Scope>> _scopes;
    std::vector<std::unique_ptr<Symbol>> _symbols;
};

} // namespace adv

#endif // ADV_SYMBOL_HPP
