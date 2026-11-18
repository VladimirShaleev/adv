#include "symbol.hpp"
#include "context.hpp"
#include "options.hpp"
#include "visitor.hpp"

using namespace std::string_view_literals;

namespace adv {

void SymbolTable::init(const Options& options, Context& ctx) {
    auto addBaseType = [&ctx](vm_value_type_t type) {
        const auto handle = ctx.intern(getTypeString(type)).handle;
        assert(handle < VM_VALUE_TYPE_COUNT_ENUMS && "the SymbolTable must be initialized before starting the parser.");
    };
    for (int i = 1; i < VM_VALUE_TYPE_COUNT_ENUMS; ++i) {
        addBaseType((vm_value_type_t)i);
    }

    _glocalScope = allocScope(Scope::Undefined);
    _scope = _glocalScope;

    for (int i = 1; i < VM_VALUE_TYPE_COUNT_ENUMS; ++i) {
        adv_node_t node{ADV_NODE_VAR_DECL};
        node.var_decl.name = vm_string_h{static_cast<adv_uint32_t>(i)};
        auto symbol = addSymbol(options, ctx, &node);
        symbol->kind = ADV_SYMBOL_KIND_PRIMITIVE;
        symbol->primitive_type = static_cast<vm_value_type_t>(i);
        _primitives[i] = node.var_decl.symbol;
    }

    adv_node_t nodeVoid{ADV_NODE_VAR_DECL};
    auto symbol = addSymbol(options, ctx, &nodeVoid);
    symbol->kind = ADV_SYMBOL_KIND_VOID;
    _void = nodeVoid.var_decl.symbol;
}

void SymbolTable::beginScope() { _scope = allocScope(_scope); }

void SymbolTable::endScope() { _scope = getScope(_scope)->parent; }

adv_scope_t* SymbolTable::getRawScope(adv_scope_h handle) noexcept {
    auto scope = getScope(handle);
    if (scope) {
        scope->children = scope->linearChildren.data();
        scope->symbols = scope->linearSymbols.data();
        scope->child_count = static_cast<adv_uint16_t>(scope->linearChildren.size());
        scope->symbol_count = static_cast<adv_uint16_t>(scope->linearSymbols.size());
    }
    return scope;
}

adv_symbol_t* SymbolTable::getRawSymbol(adv_symbol_h handle) noexcept {
    auto sym = getSymbol(handle);
    if (sym) {
        sym->children = sym->linearChildren.data();
        sym->child_count = static_cast<adv_uint16_t>(sym->linearChildren.size());
    }
    return sym;
}

adv_scope_h SymbolTable::getGlobalScope() const noexcept { return _glocalScope; }

Scope* SymbolTable::getScope(adv_scope_h handle) noexcept {
    if (handle.handle > 0) {
        const auto index = handle.handle - 1u;
        return index < _scopePool.size() ? &_scopePool[index] : nullptr;
    }
    return nullptr;
}

Symbol* SymbolTable::getSymbol(adv_symbol_h handle) noexcept {
    if (handle.handle > 0) {
        const auto index = handle.handle - 1u;
        return index < _symbolPool.size() ? &_symbolPool[index] : nullptr;
    }
    return nullptr;
}

adv_symbol_h SymbolTable::lookupVoid() noexcept { return _void; }

adv_symbol_h SymbolTable::lookup(vm_value_type_t primitiveType) noexcept { return _primitives[primitiveType]; }

adv_symbol_h SymbolTable::lookup(adv_scope_h scope, vm_string_h name) noexcept {
    if (name.handle > VM_VALUE_TYPE_UNKNOWN && name.handle < VM_VALUE_TYPE_COUNT_ENUMS) {
        return lookup(static_cast<vm_value_type_t>(name.handle));
    }
    auto scopePtr = getScope(scope);
    while (scopePtr) {
        if (auto sym = scopePtr->symbolMap.find(name); sym != scopePtr->symbolMap.end()) {
            return sym->second;
        }
        scopePtr = getScope(scopePtr->parent);
    }
    return Symbol::Undefined;
}

std::pair<adv_symbol_h, Symbol*>
SymbolTable::lookup(const Options& options, Context& ctx, const adv_location_t& loc, vm_string_h name) noexcept {
    auto scope = getScope(_scope);
    while (scope) {
        if (auto sym = scope->symbolMap.find(name); sym != scope->symbolMap.end()) {
            return {sym->second, getSymbol(sym->second)};
        }
        scope = getScope(scope->parent);
    }
    ctx.log<ADV_STATUS_E3009>(options, loc, name);
    return {Symbol::Undefined, nullptr};
}

Symbol* SymbolTable::addSymbol(const Options& options, Context& ctx, adv_node_t* node) {
    auto scope = getScope(_scope);
    auto handle = allocSymbol(node);
    auto sym = getSymbol(handle);
    if (auto existsSym = scope->symbolMap.find(sym->name); existsSym != scope->symbolMap.end()) {
        const auto& location = getSymbol(existsSym->second)->location;
        ctx.log<ADV_STATUS_E3008>(options, sym->location, sym->name, location);
        return sym;
    }
    if (sym->kind != ADV_SYMBOL_KIND_FIELD) {
        auto currScope = getScope(scope->parent);
        while (currScope) {
            if (auto existsSym = currScope->symbolMap.find(sym->name); existsSym != currScope->symbolMap.end()) {
                const auto& location = getSymbol(existsSym->second)->location;
                ctx.log<ADV_STATUS_N1001>(options, sym->location, sym->name, location);
                break;
            }
            currScope = getScope(currScope->parent);
        }
    }
    if (sym->kind == ADV_SYMBOL_KIND_PARAMETER) {
        auto fnSym = getSymbol(node->parent->func_decl.symbol);
        fnSym->linearChildren.push_back(handle);
    } else if (sym->kind == ADV_SYMBOL_KIND_FIELD) {
        auto structSym = getSymbol(node->parent->struct_decl.symbol);
        structSym->linearChildren.push_back(handle);
    }
    scope->linearSymbols.push_back(handle);
    scope->symbolMap[sym->name] = handle;
    return sym;
}

void SymbolTable::getVisibleSymbolsAt(
    const adv_location_t& loc, adv_symbol_h* symbols, adv_uint32_t& symbolCount) noexcept {}

adv_scope_h SymbolTable::allocScope(adv_scope_h parent) {
    const adv_scope_h handle = {static_cast<decltype(adv_scope_h::handle)>(_scopePool.size() + 1u)};
    _scopePool.emplace_back();
    auto& scope = _scopePool.back();
    scope.parent = parent;
    if (parent != Scope::Undefined) {
        auto parentScope = getScope(parent);
        parentScope->linearChildren.push_back(handle);
    }
    return handle;
}

adv_symbol_h SymbolTable::allocSymbol(adv_node_t* node) {
    struct KindVisitor {
        explicit KindVisitor(adv_symbol_h symbol) noexcept : symbol(symbol) {}
        void visit(adv_node_t* node, Tag<ADV_NODE_VAR_DECL>) noexcept {
            set(ADV_SYMBOL_KIND_VARIABLE, node->var_decl.name, node->var_decl.symbol);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_NATIVE_DECL>) noexcept {
            set(ADV_SYMBOL_KIND_NATIVE, node->func_decl.name, node->func_decl.symbol);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_FUNCTION_DECL>) noexcept {
            set(ADV_SYMBOL_KIND_FUNCTION, node->func_decl.name, node->func_decl.symbol);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_STRUCT_DECL>) noexcept {
            set(ADV_SYMBOL_KIND_STRUCTURE, node->struct_decl.name, node->struct_decl.symbol);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_FIELD_DECL>) noexcept {
            set(ADV_SYMBOL_KIND_FIELD, node->field_decl.name, node->field_decl.symbol);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_PARAM_DECL>) noexcept {
            set(ADV_SYMBOL_KIND_PARAMETER, node->param_decl.name, node->param_decl.symbol);
        }
        void visit(...) noexcept { assert(!"unreachable code"); }
        void set(adv_symbol_kind_t nodeKind, vm_string_h nodeName, adv_symbol_h& nodeSymbol) noexcept {
            kind = nodeKind, name = nodeName, nodeSymbol = symbol;
        }
        adv_symbol_h symbol{};
        vm_string_h name{};
        adv_symbol_kind_t kind{};
    };
    const adv_symbol_h handle = {static_cast<decltype(adv_symbol_h::handle)>(_symbolPool.size() + 1u)};
    const auto visitor = accept<KindVisitor>(node, handle);

    _symbolPool.emplace_back();
    auto& sym = _symbolPool.back();
    sym.name = visitor.name;
    sym.kind = visitor.kind;
    sym.primitive_type = VM_VALUE_TYPE_UNKNOWN;
    sym.type = Undefined;
    sym.location = node->location;
    sym.scope = _scope;
    sym.is_global = _scope == _glocalScope ? 1 : 0;
    return handle;
}

std::string_view SymbolTable::getTypeString(vm_value_type_t type) noexcept {
    switch (type) {
    case VM_VALUE_TYPE_BOOL:
        return "bool"sv;
    case VM_VALUE_TYPE_INT:
        return "int"sv;
    case VM_VALUE_TYPE_FLOAT:
        return "float"sv;
    case VM_VALUE_TYPE_STRING:
        return "string"sv;
    default:
        return "<error>"sv;
    }
}

} // namespace adv
