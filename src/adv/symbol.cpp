#include "symbol.hpp"
#include "context.hpp"
#include "options.hpp"
#include "visitor.hpp"

namespace adv {

namespace {

template <adv_status_t Status>
void log(const Options& options, ContextBase& ctx, const Symbol& defSym, const Symbol& redefSym) {
    const auto warnAsError = options.getWarningsAsErrors();

    const auto& loc = redefSym.location;
    const auto name = ctx.getString(redefSym.name);
    const auto defLoc =
        std::format("{}:{}:{}", ctx.getString(defSym.location.filename), defSym.location.line, defSym.location.column);
    ctx.addMessage(Status, warnAsError, loc.filename, loc.line, loc.column, [name, &defLoc]() {
        return err<Status>(name, defLoc);
    });
}

template <adv_status_t Status>
void log(const Options& options, ContextBase& ctx, const adv_location_t& loc, std::string_view name) {
    const auto warnAsError = options.getWarningsAsErrors();
    ctx.addMessage(Status, warnAsError, loc.filename, loc.line, loc.column, [name]() { return err<Status>(name); });
}

} // namespace

SymbolTable::SymbolTable() : _glocalScope(allocScope(nullptr)), _scope(_glocalScope) {}
void SymbolTable::beginScope() { _scope = allocScope(_scope); }
void SymbolTable::endScope() { _scope = _scope->parent; }
void SymbolTable::setScope(void* scope) { _scope = (Scope*)scope; }

bool SymbolTable::addSymbol(const Options& options, ContextBase& ctx, adv_node_t* node) {
    auto sym = allocSym(node);
    if (auto existsSym = _scope->symbols.find(sym->name); existsSym != _scope->symbols.end()) {
        log<ADV_STATUS_E3008>(options, ctx, *existsSym->second, *sym);
        return false;
    }
    auto currScope = _scope;
    while (currScope) {
        if (auto existsSym = currScope->symbols.find(sym->name); existsSym != currScope->symbols.end()) {
            log<ADV_STATUS_N1001>(options, ctx, *existsSym->second, *sym);
            break;
        }
        currScope = currScope->parent;
    }
    if (sym->kind == SymbolKind::Param) {
        auto fnSym = lookup(options, ctx, node->parent->location, node->parent->fn_decl.name);
        fnSym->params.push_back(sym);
    }
    _scope->symbols[sym->name] = sym;
    return true;
}

Symbol* SymbolTable::lookup(
    const Options& options, ContextBase& ctx, const adv_location_t& loc, vm_string_h name) const noexcept {
    auto scope = _scope;
    while (scope) {
        if (auto sym = scope->symbols.find(name); sym != scope->symbols.end()) {
            return sym->second;
        }
        scope = scope->parent;
    }
    log<ADV_STATUS_E3009>(options, ctx, loc, ctx.getString(name));
    return nullptr;
}

void* SymbolTable::getScope() noexcept { return (void*)_scope; }

Scope* SymbolTable::allocScope(Scope* parentScope) {
    return _scopes.emplace_back(std::make_unique<Scope>(parentScope)).get();
}

Symbol* SymbolTable::allocSym(adv_node_t* node) {
    struct KindVisitor {
        using K = SymbolKind;
        explicit KindVisitor(Scope* scope) noexcept : scope((void*)scope) {}
        void visit(adv_node_t* n, Tag<ADV_NODE_VAR_DECL>) noexcept { set(K::Var, n->var_decl.scope); }
        void visit(adv_node_t* n, Tag<ADV_NODE_FUNCTION_DECL>) noexcept { set(K::Function, n->fn_decl.scope); }
        void visit(adv_node_t* n, Tag<ADV_NODE_PARAM_DECL>) noexcept { set(K::Param, n->param_decl.scope); }
        void visit(...) noexcept { assert(!"unreachable code"); }
        void set(K nodeKind, void*& nodeScope) noexcept { kind = nodeKind, nodeScope = scope; }
        void* scope{};
        K kind{};
    };
    const auto kind = accept<KindVisitor>(node, _scope).kind;
    auto symbol     = _symbols.emplace_back(std::make_unique<Symbol>()).get();

    symbol->name = node->var_decl.name;
    symbol->kind = kind;
    // symbol-> valType;
    symbol->location       = node->location;
    symbol->frameSlotCount = 0;
    symbol->slotIndex      = 0;
    symbol->isGlobal       = _scope == _glocalScope;
    return symbol;
}

} // namespace adv
