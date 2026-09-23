#include "context.hpp"
#include "parser.hpp"
#include "visitor.hpp"

using namespace std::literals;

namespace adv {

ContextBase::ContextBase() {
    size_t i = 0;

    _baseTypes[i++] = std::make_pair(vm_string_h{}, VM_VALUE_TYPE_UNKNOWN);
    _baseTypes[i++] = std::make_pair(intern("bool"sv), VM_VALUE_TYPE_BOOL);
    _baseTypes[i++] = std::make_pair(intern("int"sv), VM_VALUE_TYPE_INT);
    _baseTypes[i++] = std::make_pair(intern("float"sv), VM_VALUE_TYPE_FLOAT);
    _baseTypes[i++] = std::make_pair(intern("string"sv), VM_VALUE_TYPE_STRING);
}

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

void ContextBase::build(const Options& options) {
    if (!hasErrors()) {
        resolveSymbols(options);
    }
    if (!hasErrors()) {
        resolveTypes(options);
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

void ContextBase::resolveSymbols(const Options& options) {
    struct SymbolVisitor {
        SymbolVisitor(const Options& options, ContextBase& ctx, SymbolTable& symbolTable, adv_node_t* parent) noexcept
            : options(options), ctx(ctx), symbolTable(symbolTable), parent(parent) {}

        void visit(adv_node_t* node, Tag<ADV_NODE_PROGRAM>) {
            node->parent = parent;
            visitChildrens(node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_IMPORT_STMT>) {
            node->parent = parent;
            acceptNode(node->import_stmt.root, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_VAR_DECL>) {
            node->parent = parent;
            symbolTable.addSymbol(options, ctx, node);
            acceptNode(node->var_decl.init_expr, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_FUNCTION_DECL>) {
            node->parent = parent;
            symbolTable.addSymbol(options, ctx, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_PARAM_DECL>) {
            node->parent = parent;
            symbolTable.addSymbol(options, ctx, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_IDENTIFIER_EXPR>) {
            node->parent = parent;
            if (auto sym = symbolTable.lookup(options, ctx, node->location, node->id_expr.name)) {
                node->id_expr.scope = symbolTable.getScope();
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_RETURN_STMT>) {
            node->parent = parent;
            acceptNode(node->unary.operand, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_EXPR_STMT>) {
            node->parent = parent;
            visitChildrens(node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_ASSIGN_EXPR>) {
            node->parent = parent;
            acceptNode(node->assign.val, node);
            acceptNode(node->assign.target, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_BINARY_EXPR>) {
            node->parent = parent;
            acceptNode(node->binop.left, node);
            acceptNode(node->binop.right, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_UNARY_EXPR>) {
            node->parent = parent;
            acceptNode(node->unary.operand, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_CALL_EXPR>) {
            node->parent = parent;
            auto curr    = node->call.first_arg;
            while (curr) {
                acceptNode(curr, node);
                curr = curr->next_sibling;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_EXPLICIT_CAST_EXPR>) {
            node->parent = parent;
            acceptNode(node->cast_expr.operand, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_BLOCK>) {
            node->parent = parent;
            symbolTable.beginScope();
            visitChildrens(node);
            symbolTable.endScope();
        }
        void visit(...) noexcept {}

        void visitChildrens(adv_node_t* node) {
            ctx.visitChildrens(node, [this](auto node, auto parent) { acceptNode(node, parent); }, true);
        }

        void acceptNode(adv_node_t* node, adv_node_t* parent) {
            accept<SymbolVisitor>(node, std::cref(options), std::ref(ctx), std::ref(symbolTable), parent);
        }

        const Options& options;
        ContextBase& ctx;
        SymbolTable& symbolTable;
        adv_node_t* parent;
    };

    accept<SymbolVisitor>(root(), std::cref(options), std::ref(*this), std::ref(_symbolTable), nullptr);
}

void ContextBase::resolveTypes(const Options& options) {
    struct TypeVisitor {
        TypeVisitor(const Options& options, SymbolTable& symbolTable, ContextBase& ctx) noexcept
            : options(options), symbolTable(symbolTable), ctx(ctx) {}
        void visit(adv_node_t* node, Tag<ADV_NODE_PROGRAM>) { visitChildrens(node); }
        void visit(adv_node_t* node, Tag<ADV_NODE_IMPORT_STMT>) { acceptNode(node->import_stmt.root); }
        void visit(adv_node_t* node, Tag<ADV_NODE_INT_LITERAL>) { node->resolved_type = VM_VALUE_TYPE_INT; }
        void visit(adv_node_t* node, Tag<ADV_NODE_FLOAT_LITERAL>) { node->resolved_type = VM_VALUE_TYPE_FLOAT; }
        void visit(adv_node_t* node, Tag<ADV_NODE_STRING_LITERAL>) { node->resolved_type = VM_VALUE_TYPE_STRING; }
        void visit(adv_node_t* node, Tag<ADV_NODE_BOOL_LITERAL>) { node->resolved_type = VM_VALUE_TYPE_BOOL; }
        void visit(adv_node_t* node, Tag<ADV_NODE_FUNCTION_DECL>) {
            auto returnType = VM_VALUE_TYPE_VOID;
            if (node->fn_decl.return_type_hint.handle != 0) {
                returnType = ctx.findBaseType(node->fn_decl.return_type_hint);
                if (returnType == VM_VALUE_TYPE_UNKNOWN) {
                    ctx.log<ADV_STATUS_E3011>(
                        options,
                        node->location,
                        ctx.getString(node->fn_decl.return_type_hint),
                        ctx.getString(node->fn_decl.name));
                    return;
                }
            }
            symbolTable.setScope(node->fn_decl.scope);
            auto sym  = symbolTable.lookup(options, ctx, node->location, node->fn_decl.name);
            sym->type = returnType;

            node->resolved_type = returnType;

            auto param = node->fn_decl.first_param;
            while (param) {
                acceptNode(param);
                param = param->next_sibling;
            }
            // acceptNode(node->fn_decl.body);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_RETURN_STMT>) {
            const bool hasExpr = node->unary.operand != nullptr;
            auto retType       = VM_VALUE_TYPE_UNKNOWN;
            if (hasExpr) {
                retType = acceptNode(node->unary.operand);
                if (retType == VM_VALUE_TYPE_UNKNOWN) {
                    return;
                }
            }
            auto& fn = node->parent->parent;
            if (fn->resolved_type == VM_VALUE_TYPE_VOID && hasExpr) {
                ctx.log<ADV_STATUS_E3013>(options, node->location, ctx.getString(fn->fn_decl.name));
                return;
            }
            if (fn->resolved_type != VM_VALUE_TYPE_VOID && !hasExpr) {
                ctx.log<ADV_STATUS_E3014>(options, node->location, ctx.getString(fn->fn_decl.name));
                return;
            }
            if (fn->resolved_type != retType) {
                auto result = ctx.getCastResult(retType, fn->resolved_type, false);
                if (result == CastResult::WarningTruncate) {
                    ctx.log<ADV_STATUS_W2002>(
                        options,
                        node->location,
                        ctx.getTypeString(retType),
                        ctx.getString(fn->fn_decl.return_type_hint));
                } else if (result == CastResult::ErrorForbidden) {
                    ctx.log<ADV_STATUS_E3015>(
                        options,
                        node->location,
                        ctx.getTypeString(retType),
                        ctx.getString(fn->fn_decl.return_type_hint),
                        ctx.getString(fn->fn_decl.name));
                    return;
                }
                node->unary.operand = ctx.promoteNode(node->unary.operand, fn->resolved_type);
                node->resolved_type = fn->resolved_type;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_PARAM_DECL>) {
            const bool hasTypeHint = node->param_decl.type_hint.handle != 0;
            if (!hasTypeHint) {
                ctx.log<ADV_STATUS_E3016>(options, node->location, ctx.getString(node->param_decl.name));
                return;
            }
            const auto paramType = ctx.findBaseType(node->param_decl.type_hint);
            if (paramType == VM_VALUE_TYPE_UNKNOWN) {
                ctx.log<ADV_STATUS_E3017>(
                    options,
                    node->location,
                    ctx.getString(node->param_decl.type_hint),
                    ctx.getString(node->param_decl.name));
                return;
            }
            symbolTable.setScope(node->param_decl.scope);
            if (auto sym = symbolTable.lookup(options, ctx, node->location, node->param_decl.name)) {
                sym->type = paramType;
            }
            node->resolved_type = paramType;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_IDENTIFIER_EXPR>) {
            symbolTable.setScope(node->id_expr.scope);
            auto* sym = symbolTable.lookup(options, ctx, node->location, node->id_expr.name);
            if (sym) {
                if (sym->type == VM_VALUE_TYPE_UNKNOWN) {
                    ctx.log<ADV_STATUS_E3018>(options, node->location, ctx.getString(node->id_expr.name));
                    return;
                }
                // node->id_expr.type_hint = sym->
                node->resolved_type = sym->type;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_BINARY_EXPR>) {
            const auto leftType  = acceptNode(node->binop.left);
            const auto rightType = acceptNode(node->binop.right);
            if (leftType == VM_VALUE_TYPE_UNKNOWN || rightType == VM_VALUE_TYPE_UNKNOWN) {
                return;
            }
            auto toType = leftType;
            if (leftType != rightType) {
                const auto needSwap = (int)rightType < (int)leftType;
                const auto from     = needSwap ? rightType : leftType;
                const auto to       = needSwap ? leftType : rightType;
                const auto result   = ctx.getCastResult(from, to, false);
                if (result == CastResult::WarningTruncate) {
                    ctx.log<ADV_STATUS_W2003>(options, node->location, ctx.getTypeString(from), ctx.getTypeString(to));
                } else if (result == CastResult::ErrorForbidden) {
                    ctx.log<ADV_STATUS_E3019>(options, node->location, ctx.getTypeString(from), ctx.getTypeString(to));
                    return;
                }
                if (leftType == from) {
                    node->binop.left = ctx.promoteNode(node->binop.left, to);
                } else {
                    node->binop.right = ctx.promoteNode(node->binop.right, to);
                }
                toType = to;
            }
            if (isComparisonOp(node->binop.op)) {
                node->resolved_type = VM_VALUE_TYPE_BOOL;
                return;
            }
            node->resolved_type = toType;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_CALL_EXPR>) {
            auto* fnSym = symbolTable.lookup(options, ctx, node->location, node->call.name);
            if (!fnSym) {
                ctx.log<ADV_STATUS_E3020>(options, node->location, ctx.getString(node->call.name));
                return;
            }
            if (fnSym->kind != SymbolKind::Function) {
                ctx.log<ADV_STATUS_E3021>(options, node->location, ctx.getString(node->call.name));
                return;
            }
            node->resolved_type = fnSym->type;

            adv_node_t** argPtr = &node->call.first_arg;
            const auto& params  = fnSym->params;
            if (params.size() != node->call.arg_count) {
                ctx.log<ADV_STATUS_E3022>(
                    options, node->location, ctx.getString(node->call.name), params.size(), node->call.arg_count);
                return;
            }
            for (const auto param : params) {
                auto arg             = *argPtr;
                const auto argType   = acceptNode(arg);
                const auto paramType = param->type;
                if (argType != paramType) {
                    const auto result = ctx.getCastResult(argType, paramType, false);
                    if (result == CastResult::WarningTruncate) {
                        ctx.log<ADV_STATUS_W2004>(
                            options,
                            arg->location,
                            ctx.getTypeString(argType),
                            ctx.getTypeString(paramType),
                            ctx.getString(param->name),
                            ctx.getString(node->call.name));
                    } else if (result == CastResult::ErrorForbidden) {
                        ctx.log<ADV_STATUS_E3023>(
                            options,
                            arg->location,
                            ctx.getString(param->name),
                            ctx.getTypeString(argType),
                            ctx.getTypeString(paramType),
                            ctx.getString(node->call.name));
                        continue;
                    }
                    *argPtr                 = ctx.promoteNode(arg, paramType);
                    (*argPtr)->next_sibling = arg->next_sibling;
                    arg->next_sibling       = nullptr;
                }
                argPtr = &(*argPtr)->next_sibling;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_UNARY_EXPR>) {
            node->resolved_type = acceptNode(node->unary.operand);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_ASSIGN_EXPR>) {
            const auto targetType = acceptNode(node->assign.target);
            if (targetType == VM_VALUE_TYPE_UNKNOWN) {
                return;
            }
            const auto valueType = acceptNode(node->assign.val);
            if (valueType == VM_VALUE_TYPE_UNKNOWN) {
                return;
            }
            if (targetType != valueType) {
                auto result = ctx.getCastResult(valueType, targetType, false);
                if (result == CastResult::WarningTruncate) {
                    ctx.log<ADV_STATUS_W2005>(
                        options, node->location, ctx.getTypeString(valueType), ctx.getTypeString(targetType));
                } else if (result == CastResult::ErrorForbidden) {
                    ctx.log<ADV_STATUS_E3024>(
                        options, node->location, ctx.getTypeString(valueType), ctx.getTypeString(targetType));
                    return;
                }
                node->assign.val = ctx.promoteNode(node->assign.val, targetType);
            }
            node->resolved_type = targetType;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_VAR_DECL>) {
            symbolTable.setScope(node->var_decl.scope);
            auto sym = symbolTable.lookup(options, ctx, node->location, node->var_decl.name);
            if (sym->type != VM_VALUE_TYPE_UNKNOWN) {
                return;
            }
            const bool hasTypeHint = node->var_decl.type_hint.handle != 0;
            const bool hasInitExpr = node->var_decl.init_expr != nullptr;
            if (!hasTypeHint && !hasInitExpr) {
                ctx.log<ADV_STATUS_E3025>(options, node->location, ctx.getString(node->var_decl.name));
                return;
            }
            auto hintType = VM_VALUE_TYPE_UNKNOWN;
            auto initType = VM_VALUE_TYPE_UNKNOWN;
            if (hasTypeHint) {
                hintType = ctx.findBaseType(node->var_decl.type_hint);
                if (hintType == VM_VALUE_TYPE_UNKNOWN) {
                    ctx.log<ADV_STATUS_E3026>(
                        options,
                        node->location,
                        ctx.getString(node->var_decl.type_hint),
                        ctx.getString(node->var_decl.name));
                    return;
                }
            }
            if (hasInitExpr) {
                initType = acceptNode(node->var_decl.init_expr);
                if (initType == VM_VALUE_TYPE_UNKNOWN) {
                    return;
                }
            }
            if (hasTypeHint && hasInitExpr && hintType != initType) {
                auto result = ctx.getCastResult(initType, hintType, false);
                if (result == CastResult::WarningTruncate) {
                    ctx.log<ADV_STATUS_W2006>(
                        options,
                        node->location,
                        ctx.getTypeString(initType),
                        ctx.getTypeString(hintType),
                        ctx.getString(node->var_decl.name));
                } else if (result == CastResult::ErrorForbidden) {
                    ctx.log<ADV_STATUS_E3027>(
                        options,
                        node->location,
                        ctx.getString(node->var_decl.name),
                        ctx.getTypeString(initType),
                        ctx.getTypeString(hintType));
                    return;
                }
                node->var_decl.init_expr = ctx.promoteNode(node->var_decl.init_expr, hintType);
            }
            const auto finalType = hasTypeHint ? hintType : initType;

            sym->type           = finalType;
            node->resolved_type = finalType;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_EXPLICIT_CAST_EXPR>) {
            const auto srcType = acceptNode(node->cast_expr.operand);
            if (srcType == VM_VALUE_TYPE_UNKNOWN) {
                return;
            }
            const auto targetType = ctx.findBaseType(node->cast_expr.target_type_hint);
            if (targetType == VM_VALUE_TYPE_UNKNOWN) {
                ctx.log<ADV_STATUS_E3010>(options, node->location, ctx.getString(node->cast_expr.target_type_hint));
                return;
            }
            const auto result = ctx.getCastResult(srcType, targetType, true);
            if (result == CastResult::ErrorForbidden) {
                ctx.log<ADV_STATUS_E3012>(
                    options,
                    node->location,
                    ctx.getTypeString(srcType),
                    ctx.getString(node->cast_expr.target_type_hint));
                return;
            }
            node->cast_expr.from_type = srcType;
            node->cast_expr.to_type   = targetType;
            node->resolved_type       = targetType;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_BLOCK>) { visitChildrens(node); }
        void visit(...) noexcept { int i = 5; }
        void visitChildrens(adv_node_t* node) {
            ctx.visitChildrens(node, [this](auto node, auto /* parent */) { acceptNode(node); });
        }
        vm_value_type_t acceptNode(adv_node_t* node) {
            accept<TypeVisitor>(node, std::cref(options), std::ref(symbolTable), std::ref(ctx));
            return node->resolved_type;
        }
        const Options& options;
        SymbolTable& symbolTable;
        ContextBase& ctx;
    };
    accept<TypeVisitor>(root(), std::cref(options), std::ref(_symbolTable), std::ref(*this));
}

bool ContextBase::isComparisonOp(adv_uint32_t op) noexcept {
    switch (op) {
    case '<':
    case '>':
    case Parser::token::LE:
    case Parser::token::GE:
    case Parser::token::EQ:
    case Parser::token::NE:
        return true;
    default:
        return false;
    }
}

CastResult ContextBase::getCastResult(vm_value_type_t from, vm_value_type_t to, bool isExplicitAs) noexcept {
    if (from == to) {
        return CastResult::OK;
    }
    if (from == VM_VALUE_TYPE_INT && to == VM_VALUE_TYPE_FLOAT) {
        return CastResult::OK;
    }
    if (from == VM_VALUE_TYPE_BOOL && to == VM_VALUE_TYPE_INT) {
        return CastResult::OK;
    }
    if (from == VM_VALUE_TYPE_FLOAT && to == VM_VALUE_TYPE_INT) {
        return isExplicitAs ? CastResult::OK : CastResult::WarningTruncate;
    }
    if (from == VM_VALUE_TYPE_INT && to == VM_VALUE_TYPE_BOOL) {
        return isExplicitAs ? CastResult::OK : CastResult::WarningTruncate;
    }
    if (to == VM_VALUE_TYPE_STRING || from == VM_VALUE_TYPE_STRING) {
        return isExplicitAs ? CastResult::OK : CastResult::ErrorForbidden;
    }
    return CastResult::ErrorForbidden;
}

std::string_view ContextBase::getTypeString(vm_value_type_t type) noexcept {
    switch (type) {
    case VM_VALUE_TYPE_BOOL:
        return "bool"sv;
    case VM_VALUE_TYPE_INT:
        return "int"sv;
    case VM_VALUE_TYPE_FLOAT:
        return "float"sv;
    case VM_VALUE_TYPE_STRING:
        return "string"sv;
    case VM_VALUE_TYPE_VOID:
        return "void"sv;
    default:
        return "<error>"sv;
    }
}

vm_value_type_t ContextBase::findBaseType(vm_string_h type) const noexcept {
    return type.handle < _baseTypes.size() ? _baseTypes[type.handle].second : VM_VALUE_TYPE_UNKNOWN;
}

adv_node_t* ContextBase::promoteNode(adv_node_t* node, vm_value_type_t targetType) {
    if (!node || node->resolved_type == targetType) {
        return node;
    }
    auto castNode = allocNode(node->location, ADV_NODE_IMPLICIT_CAST_EXPR);

    castNode->cast_expr.operand   = node;
    castNode->cast_expr.from_type = node->resolved_type;
    castNode->cast_expr.to_type   = targetType;
    castNode->resolved_type       = targetType;
    return castNode;
}

void ContextBase::visitChildrens(
    adv_node_t* node, std::function<void(adv_node_t*, adv_node_t*)> accept, bool useScopes) {
    auto hasFunctions = false;

    auto curr = node->children.first_child;
    while (curr) {
        accept(curr, node);
        if (curr->type == ADV_NODE_FUNCTION_DECL) {
            hasFunctions = true;
        }
        curr = curr->next_sibling;
    }
    if (hasFunctions) {
        curr = node->children.first_child;
        while (curr) {
            if (curr->type == ADV_NODE_FUNCTION_DECL) {
                if (useScopes) {
                    _symbolTable.beginScope();
                    auto param = curr->fn_decl.first_param;
                    while (param) {
                        accept(param, curr);
                        param = param->next_sibling;
                    }
                }
                accept(curr->fn_decl.body, curr);
                if (useScopes) {
                    _symbolTable.endScope();
                }
            }
            curr = curr->next_sibling;
        }
    }
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
