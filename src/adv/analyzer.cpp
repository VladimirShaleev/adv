#include "analyzer.hpp"
#include "context.hpp"
#include "options.hpp"
#include "parser.hpp"
#include "visitor.hpp"

using namespace std::literals;

namespace adv {

Analyzer::Analyzer(const Options& options, Context& ctx) noexcept : _options(options), _ctx(ctx) {
    auto addBaseType = [this](vm_value_type_t type) {
        const auto handle = _ctx.intern(getTypeString(type)).handle;
        assert(handle < VM_VALUE_TYPE_COUNT_ENUMS && "the Analyzer must be initialized before starting the parser.");
    };
    for (int i = 1; i < VM_VALUE_TYPE_COUNT_ENUMS; ++i) {
        addBaseType((vm_value_type_t)i);
    }
}

adv_result_t Analyzer::build() {
    if (_ctx.hasErrors()) {
        return ADV_RESULT_COMPILATION_FAILED;
    }
    if (!_ctx.bytecode().empty()) {
        return ADV_RESULT_SUCCESS;
    }
    resolveSymbols();

    if (_ctx.hasErrors()) {
        return ADV_RESULT_COMPILATION_FAILED;
    }
    resolveTypes();

    return _ctx.hasErrors() ? ADV_RESULT_COMPILATION_FAILED : ADV_RESULT_SUCCESS;
}

void Analyzer::resolveSymbols() {
    struct SymbolVisitor {
        SymbolVisitor(
            const Options& options,
            Context& ctx,
            Analyzer& analyzer,
            SymbolTable& symbolTable,
            adv_node_t* parent) noexcept
            : options(options), ctx(ctx), analyzer(analyzer), symbolTable(symbolTable), parent(parent) {}

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
            auto sym = symbolTable.addSymbol(options, ctx, node);
            acceptNode(node->var_decl.init_expr, node);
            analyzer.saveFrameSlot(sym);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_FUNCTION_DECL>) {
            node->parent = parent;
            symbolTable.addSymbol(options, ctx, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_PARAM_DECL>) {
            node->parent = parent;
            auto sym = symbolTable.addSymbol(options, ctx, node);
            analyzer.saveFrameSlot(sym);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_IDENTIFIER_EXPR>) {
            auto [handle, _] = symbolTable.lookup(options, ctx, node->location, node->id_expr.name);
            node->parent = parent;
            node->id_expr.symbol = handle;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_RETURN_STMT>) {
            node->parent = parent;
            acceptNode(node->expr_stmt.expr, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_EXPR_STMT>) {
            node->parent = parent;
            acceptNode(node->expr_stmt.expr, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_ASSIGN_EXPR>) {
            node->parent = parent;
            acceptNode(node->assign_expr.val, node);
            acceptNode(node->assign_expr.target, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_BINARY_EXPR>) {
            node->parent = parent;
            acceptNode(node->binary_expr.left, node);
            acceptNode(node->binary_expr.right, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_UNARY_EXPR>) {
            node->parent = parent;
            acceptNode(node->unary_expr.operand, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_CALL_EXPR>) {
            node->parent = parent;
            auto curr = node->call_expr.first_arg;
            while (curr) {
                acceptNode(curr, node);
                curr = curr->next_sibling;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_EXPLICIT_CAST_EXPR>) {
            node->parent = parent;
            acceptNode(node->cast_expr.operand, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_IF_STMT>) {
            node->parent = parent;
            acceptNode(node->if_stmt.cond, node);
            acceptNode(node->if_stmt.then_branch, node);
            acceptNode(node->if_stmt.else_branch, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_WHILE_STMT>) {
            node->parent = parent;
            acceptNode(node->while_stmt.cond, node);
            acceptNode(node->while_stmt.body, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_YIELD_STMT>) { node->parent = parent; }
        void visit(adv_node_t* node, Tag<ADV_NODE_WAIT_STMT>) {
            node->parent = parent;
            acceptNode(node->expr_stmt.expr, node);
        }
        void visit(adv_node_t* /* node */, Tag<ADV_NODE_IMPLICIT_CAST_EXPR>) { assert(!"unreachable code"); }
        void visit(adv_node_t* node, Tag<ADV_NODE_MEMBER_ACCESS>) {
            node->parent = parent;
            acceptNode(node->member_access.target, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_BOOL_LITERAL>) { node->parent = parent; }
        void visit(adv_node_t* node, Tag<ADV_NODE_INT_LITERAL>) { node->parent = parent; }
        void visit(adv_node_t* node, Tag<ADV_NODE_FLOAT_LITERAL>) { node->parent = parent; }
        void visit(adv_node_t* node, Tag<ADV_NODE_STRING_LITERAL>) { node->parent = parent; }
        void visit(adv_node_t* node, Tag<ADV_NODE_BLOCK>) {
            node->parent = parent;
            adv_uint16_t slotsBeforeBlock = 0;
            const auto isNonFunctionBlock = parent->type != ADV_NODE_FUNCTION_DECL;
            if (isNonFunctionBlock) {
                symbolTable.beginScope();
                if (analyzer.hasFrame()) {
                    slotsBeforeBlock = analyzer.currFrame().currentSlot;
                }
            }
            visitChildrens(node);
            if (isNonFunctionBlock) {
                if (analyzer.hasFrame()) {
                    analyzer.currFrame().currentSlot = slotsBeforeBlock;
                }
                symbolTable.endScope();
            }
        }
        void visitChildrens(adv_node_t* node) {
            analyzer.visitStmtsWithRules(node, [this](auto node, auto parent) { acceptNode(node, parent); }, true);
        }
        void acceptNode(adv_node_t* node, adv_node_t* parent) {
            accept<SymbolVisitor>(
                node, std::cref(options), std::ref(ctx), std::ref(analyzer), std::ref(symbolTable), parent);
        }

        const Options& options;
        Context& ctx;
        Analyzer& analyzer;
        SymbolTable& symbolTable;
        adv_node_t* parent;
    };
    accept<SymbolVisitor>(
        _ctx.root(), std::cref(_options), std::ref(_ctx), std::ref(*this), std::ref(_ctx.symbolTable()), nullptr);
}

void Analyzer::resolveTypes() {
    struct TypeVisitor {
        TypeVisitor(const Options& options, Context& ctx, Analyzer& analyzer, SymbolTable& symbolTable) noexcept
            : options(options), ctx(ctx), analyzer(analyzer), symbolTable(symbolTable) {}
        void visit(adv_node_t* node, Tag<ADV_NODE_PROGRAM>) { visitChildrens(node); }
        void visit(adv_node_t* node, Tag<ADV_NODE_IMPORT_STMT>) { acceptNode(node->import_stmt.root); }
        void visit(adv_node_t* node, Tag<ADV_NODE_INT_LITERAL>) { node->resolved_type = VM_VALUE_TYPE_INT; }
        void visit(adv_node_t* node, Tag<ADV_NODE_FLOAT_LITERAL>) { node->resolved_type = VM_VALUE_TYPE_FLOAT; }
        void visit(adv_node_t* node, Tag<ADV_NODE_STRING_LITERAL>) { node->resolved_type = VM_VALUE_TYPE_STRING; }
        void visit(adv_node_t* node, Tag<ADV_NODE_BOOL_LITERAL>) { node->resolved_type = VM_VALUE_TYPE_BOOL; }
        void visit(adv_node_t* node, Tag<ADV_NODE_FUNCTION_DECL>) {
            auto returnType = VM_VALUE_TYPE_VOID;
            if (node->func_decl.return_type_hint.handle != 0) {
                returnType = analyzer.findBaseType(node->func_decl.return_type_hint);
                if (returnType == VM_VALUE_TYPE_UNKNOWN) {
                    ctx.log<ADV_STATUS_E3011>(
                        options, node->location, node->func_decl.return_type_hint, node->func_decl.name);
                    return;
                }
            }
            symbolTable.getSymbol(node->func_decl.symbol)->type = returnType;
            node->resolved_type = returnType;

            auto param = node->func_decl.first_param;
            while (param) {
                acceptNode(param);
                param = param->next_sibling;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_RETURN_STMT>) {
            const bool hasExpr = node->expr_stmt.expr != nullptr;
            auto retType = VM_VALUE_TYPE_UNKNOWN;
            if (hasExpr) {
                retType = acceptNode(node->expr_stmt.expr);
                if (retType == VM_VALUE_TYPE_UNKNOWN) {
                    return;
                }
            }
            adv_node_t* fn = node->parent;
            while (fn && fn->type != ADV_NODE_FUNCTION_DECL) {
                fn = fn->parent;
            }
            if (!fn) {
                ctx.log<ADV_STATUS_E3035>(options, node->location);
                return;
            }
            if (fn->resolved_type == VM_VALUE_TYPE_VOID && hasExpr) {
                ctx.log<ADV_STATUS_E3013>(options, node->location, fn->func_decl.name);
                return;
            }
            if (fn->resolved_type != VM_VALUE_TYPE_VOID && !hasExpr) {
                ctx.log<ADV_STATUS_E3014>(options, node->location, fn->func_decl.name);
                return;
            }
            if (fn->resolved_type != retType) {
                auto result = analyzer.getCastResult(retType, fn->resolved_type, false);
                if (result == CastResult::WarningTruncate) {
                    ctx.log<ADV_STATUS_W2002>(
                        options, node->location, analyzer.getTypeString(retType), fn->func_decl.return_type_hint);
                } else if (result == CastResult::ErrorForbidden) {
                    ctx.log<ADV_STATUS_E3015>(
                        options,
                        node->location,
                        analyzer.getTypeString(retType),
                        fn->func_decl.return_type_hint,
                        fn->func_decl.name);
                    return;
                }
                node->expr_stmt.expr = analyzer.promoteCastNode(node->expr_stmt.expr, fn->resolved_type);
                node->resolved_type = fn->resolved_type;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_EXPR_STMT>) { acceptNode(node->expr_stmt.expr); }
        void visit(adv_node_t* node, Tag<ADV_NODE_PARAM_DECL>) {
            const bool hasTypeHint = node->param_decl.type_hint.handle != 0;
            if (!hasTypeHint) {
                ctx.log<ADV_STATUS_E3016>(options, node->location, node->param_decl.name);
                return;
            }
            const auto paramType = analyzer.findBaseType(node->param_decl.type_hint);
            if (paramType == VM_VALUE_TYPE_UNKNOWN) {
                ctx.log<ADV_STATUS_E3017>(options, node->location, node->param_decl.type_hint, node->param_decl.name);
                return;
            }
            symbolTable.getSymbol(node->param_decl.symbol)->type = paramType;
            node->resolved_type = paramType;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_IDENTIFIER_EXPR>) {
            const auto sym = symbolTable.getSymbol(node->id_expr.symbol);
            if (sym) {
                if (sym->type == VM_VALUE_TYPE_UNKNOWN) {
                    ctx.log<ADV_STATUS_E3018>(options, node->location, node->id_expr.name);
                    return;
                }
                node->resolved_type = sym->type;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_BINARY_EXPR>) {
            const auto leftType = acceptNode(node->binary_expr.left);
            const auto rightType = acceptNode(node->binary_expr.right);
            if (leftType == VM_VALUE_TYPE_UNKNOWN || rightType == VM_VALUE_TYPE_UNKNOWN) {
                return;
            }
            const auto op = node->binary_expr.op;

            if (isLogicOp(op)) {
                if (leftType != VM_VALUE_TYPE_BOOL || rightType != VM_VALUE_TYPE_BOOL) {
                    ctx.log<ADV_STATUS_E3031>(
                        options, node->location, analyzer.getTypeString(leftType), analyzer.getTypeString(rightType));
                    return;
                }
                node->resolved_type = VM_VALUE_TYPE_BOOL;
                return;
            }

            if (isBitwiseOp(op) || op == '%') {
                if (leftType != VM_VALUE_TYPE_INT || rightType != VM_VALUE_TYPE_INT) {
                    ctx.log<ADV_STATUS_E3032>(
                        options, node->location, analyzer.getTypeString(leftType), analyzer.getTypeString(rightType));
                    return;
                }
                node->resolved_type = VM_VALUE_TYPE_INT;
                return;
            }

            if (isOrderingOp(op)) {
                if (leftType == VM_VALUE_TYPE_BOOL || rightType == VM_VALUE_TYPE_BOOL) {
                    ctx.log<ADV_STATUS_E3033>(options, node->location);
                    return;
                }
            }

            auto targetType = leftType;
            if (leftType != rightType) {
                const auto needSwap = static_cast<int>(rightType) < static_cast<int>(leftType);
                const auto from = needSwap ? rightType : leftType;
                const auto to = needSwap ? leftType : rightType;
                const auto result = analyzer.getCastResult(from, to, false);
                if (result == CastResult::WarningTruncate) {
                    ctx.log<ADV_STATUS_W2003>(
                        options, node->location, analyzer.getTypeString(from), analyzer.getTypeString(to));
                } else if (result == CastResult::ErrorForbidden) {
                    ctx.log<ADV_STATUS_E3019>(
                        options, node->location, analyzer.getTypeString(from), analyzer.getTypeString(to));
                    return;
                }
                if (leftType == from) {
                    node->binary_expr.left = analyzer.promoteCastNode(node->binary_expr.left, to);
                } else {
                    node->binary_expr.right = analyzer.promoteCastNode(node->binary_expr.right, to);
                }
                targetType = to;
            }

            if (isComparisonOp(node->binary_expr.op)) {
                node->resolved_type = VM_VALUE_TYPE_BOOL;
            } else {
                node->resolved_type = targetType;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_CALL_EXPR>) {
            auto [_, sym] = symbolTable.lookup(options, ctx, node->location, node->call_expr.name);
            if (!sym) {
                ctx.log<ADV_STATUS_E3020>(options, node->location, node->call_expr.name);
                return;
            }
            if (sym->kind != ADV_SYMBOL_KIND_FUNCTION) {
                ctx.log<ADV_STATUS_E3021>(options, node->location, node->call_expr.name);
                return;
            }
            node->resolved_type = sym->type;

            adv_node_t** argPtr = &node->call_expr.first_arg;
            const auto& params = sym->linearParams;
            if (params.size() != node->call_expr.arg_count) {
                ctx.log<ADV_STATUS_E3022>(
                    options, node->location, node->call_expr.name, params.size(), node->call_expr.arg_count);
                return;
            }
            auto paramView = params | std::views::transform([this](const auto& paramHandle) {
                                 return symbolTable.getSymbol(paramHandle);
                             });
            for (const auto param : paramView) {
                auto arg = *argPtr;
                const auto argType = acceptNode(arg);
                const auto paramType = param->type;
                if (argType != paramType) {
                    const auto result = analyzer.getCastResult(argType, paramType, false);
                    if (result == CastResult::WarningTruncate) {
                        ctx.log<ADV_STATUS_W2004>(
                            options,
                            arg->location,
                            analyzer.getTypeString(argType),
                            analyzer.getTypeString(paramType),
                            param->name,
                            node->call_expr.name);
                    } else if (result == CastResult::ErrorForbidden) {
                        ctx.log<ADV_STATUS_E3023>(
                            options,
                            arg->location,
                            param->name,
                            analyzer.getTypeString(argType),
                            analyzer.getTypeString(paramType),
                            node->call_expr.name);
                        continue;
                    }
                    *argPtr = analyzer.promoteCastNode(arg, paramType);
                    (*argPtr)->next_sibling = arg->next_sibling;
                    arg->next_sibling = nullptr;
                }
                argPtr = &(*argPtr)->next_sibling;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_UNARY_EXPR>) {
            auto type = acceptNode(node->unary_expr.operand);
            const auto op = node->unary_expr.op;
            if (op == Parser::token::NOT) {
                if (type != VM_VALUE_TYPE_BOOL) {
                    ctx.log<ADV_STATUS_E3028>(options, node->location, analyzer.getTypeString(type));
                    return;
                }
            } else if (op == '~') {
                if (type != VM_VALUE_TYPE_INT) {
                    ctx.log<ADV_STATUS_E3029>(options, node->location, analyzer.getTypeString(type));
                    return;
                }
            } else if (op == '-') {
                if (type != VM_VALUE_TYPE_INT && type != VM_VALUE_TYPE_FLOAT) {
                    ctx.log<ADV_STATUS_E3030>(options, node->location, analyzer.getTypeString(type));
                    return;
                }
            } else {
                assert(!"unreachable code");
            }
            node->resolved_type = type;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_ASSIGN_EXPR>) {
            const auto targetType = acceptNode(node->assign_expr.target);
            if (targetType == VM_VALUE_TYPE_UNKNOWN) {
                return;
            }
            const auto valueType = acceptNode(node->assign_expr.val);
            if (valueType == VM_VALUE_TYPE_UNKNOWN) {
                return;
            }
            if (targetType != valueType) {
                auto result = analyzer.getCastResult(valueType, targetType, false);
                if (result == CastResult::WarningTruncate) {
                    ctx.log<ADV_STATUS_W2005>(
                        options, node->location, analyzer.getTypeString(valueType), analyzer.getTypeString(targetType));
                } else if (result == CastResult::ErrorForbidden) {
                    ctx.log<ADV_STATUS_E3024>(
                        options, node->location, analyzer.getTypeString(valueType), analyzer.getTypeString(targetType));
                    return;
                }
                node->assign_expr.val = analyzer.promoteCastNode(node->assign_expr.val, targetType);
            }
            node->resolved_type = targetType;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_VAR_DECL>) {
            const bool hasTypeHint = node->var_decl.type_hint.handle != 0;
            const bool hasInitExpr = node->var_decl.init_expr != nullptr;
            if (!hasTypeHint && !hasInitExpr) {
                ctx.log<ADV_STATUS_E3025>(options, node->location, node->var_decl.name);
                return;
            }
            auto hintType = VM_VALUE_TYPE_UNKNOWN;
            auto initType = VM_VALUE_TYPE_UNKNOWN;
            if (hasTypeHint) {
                hintType = analyzer.findBaseType(node->var_decl.type_hint);
                if (hintType == VM_VALUE_TYPE_UNKNOWN) {
                    ctx.log<ADV_STATUS_E3026>(options, node->location, node->var_decl.type_hint, node->var_decl.name);
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
                auto result = analyzer.getCastResult(initType, hintType, false);
                if (result == CastResult::WarningTruncate) {
                    ctx.log<ADV_STATUS_W2006>(
                        options,
                        node->location,
                        analyzer.getTypeString(initType),
                        analyzer.getTypeString(hintType),
                        node->var_decl.name);
                } else if (result == CastResult::ErrorForbidden) {
                    ctx.log<ADV_STATUS_E3027>(
                        options,
                        node->location,
                        node->var_decl.name,
                        analyzer.getTypeString(initType),
                        analyzer.getTypeString(hintType));
                    return;
                }
                node->var_decl.init_expr = analyzer.promoteCastNode(node->var_decl.init_expr, hintType);
            }
            const auto finalType = hasTypeHint ? hintType : initType;

            symbolTable.getSymbol(node->var_decl.symbol)->type = finalType;
            node->resolved_type = finalType;

            if (!hasInitExpr) {
                adv_node_t* literal{};
                switch (finalType) {
                case VM_VALUE_TYPE_BOOL:
                    literal = ctx.allocNode(node->location, ADV_NODE_BOOL_LITERAL);
                    literal->bool_val = false;
                    break;
                case VM_VALUE_TYPE_INT:
                    literal = ctx.allocNode(node->location, ADV_NODE_INT_LITERAL);
                    literal->int_val = 0;
                    break;
                case VM_VALUE_TYPE_FLOAT:
                    literal = ctx.allocNode(node->location, ADV_NODE_FLOAT_LITERAL);
                    literal->float_val = 0.0f;
                    break;
                case VM_VALUE_TYPE_STRING:
                    literal = ctx.allocNode(node->location, ADV_NODE_STRING_LITERAL);
                    literal->str_val = {0};
                    break;
                default:
                    assert(!"unreachable code");
                    break;
                }
                literal->parent = node;
                literal->resolved_type = finalType;
                node->var_decl.init_expr = literal;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_EXPLICIT_CAST_EXPR>) {
            const auto srcType = acceptNode(node->cast_expr.operand);
            if (srcType == VM_VALUE_TYPE_UNKNOWN) {
                return;
            }
            const auto targetType = analyzer.findBaseType(node->cast_expr.target_type_hint);
            if (targetType == VM_VALUE_TYPE_UNKNOWN) {
                ctx.log<ADV_STATUS_E3010>(options, node->location, node->cast_expr.target_type_hint);
                return;
            }
            const auto result = analyzer.getCastResult(srcType, targetType, true);
            if (result == CastResult::ErrorForbidden) {
                ctx.log<ADV_STATUS_E3012>(
                    options, node->location, analyzer.getTypeString(srcType), node->cast_expr.target_type_hint);
                return;
            }
            node->cast_expr.from_type = srcType;
            node->cast_expr.to_type = targetType;
            node->resolved_type = targetType;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_BLOCK>) { visitChildrens(node); }
        void visit(adv_node_t* node, Tag<ADV_NODE_IF_STMT>) {
            const auto condType = acceptNode(node->if_stmt.cond);
            if (condType != VM_VALUE_TYPE_UNKNOWN && condType != VM_VALUE_TYPE_BOOL) {
                ctx.log<ADV_STATUS_E3034>(options, node->if_stmt.cond->location, analyzer.getTypeString(condType));
            }
            if (node->if_stmt.then_branch) {
                acceptNode(node->if_stmt.then_branch);
            }
            if (node->if_stmt.else_branch) {
                acceptNode(node->if_stmt.else_branch);
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_WHILE_STMT>) {
            const auto condType = acceptNode(node->while_stmt.cond);
            if (condType != VM_VALUE_TYPE_UNKNOWN && condType != VM_VALUE_TYPE_BOOL) {
                ctx.log<ADV_STATUS_E3036>(options, node->while_stmt.cond->location, analyzer.getTypeString(condType));
            }
            if (node->while_stmt.body) {
                acceptNode(node->while_stmt.body);
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_YIELD_STMT>) {}
        void visit(adv_node_t* node, Tag<ADV_NODE_WAIT_STMT>) {
            if (!node->expr_stmt.expr) {
                ctx.log<ADV_STATUS_E3041>(options, node->location);
                return;
            }
            const auto exprType = acceptNode(node->expr_stmt.expr);
            if (exprType == VM_VALUE_TYPE_UNKNOWN) {
                return;
            }
            if (exprType != VM_VALUE_TYPE_INT && exprType != VM_VALUE_TYPE_FLOAT) {
                ctx.log<ADV_STATUS_E3040>(options, node->expr_stmt.expr->location, analyzer.getTypeString(exprType));
                return;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_IMPLICIT_CAST_EXPR>) { assert(!"unreachable code"); }
        void visit(adv_node_t* node, Tag<ADV_NODE_MEMBER_ACCESS>) { assert(!"not implemented"); }
        void visitChildrens(adv_node_t* node) {
            analyzer.visitStmtsWithRules(node, [this](auto node, auto /* parent */) { acceptNode(node); });
        }
        vm_value_type_t acceptNode(adv_node_t* node) {
            accept<TypeVisitor>(node, std::cref(options), std::ref(ctx), std::ref(analyzer), std::ref(symbolTable));
            return node ? node->resolved_type : VM_VALUE_TYPE_UNKNOWN;
        }
        const Options& options;
        Context& ctx;
        Analyzer& analyzer;
        SymbolTable& symbolTable;
    };
    accept<TypeVisitor>(
        _ctx.root(), std::cref(_options), std::ref(_ctx), std::ref(*this), std::ref(_ctx.symbolTable()));
}

void Analyzer::visitStmtsWithRules(
    adv_node_t* node, std::function<void(adv_node_t*, adv_node_t*)> accept, bool useScopes) {
    auto hasFunctions = false;

    auto curr = node->stmts.first_stmt;
    while (curr) {
        accept(curr, node);
        if (curr->type == ADV_NODE_FUNCTION_DECL) {
            hasFunctions = true;
        }
        curr = curr->next_sibling;
    }
    if (hasFunctions) {
        auto& symbolTable = _ctx.symbolTable();
        curr = node->stmts.first_stmt;
        while (curr) {
            if (curr->type == ADV_NODE_FUNCTION_DECL) {
                if (useScopes) {
                    symbolTable.beginScope();
                    pushFrame();
                    auto param = curr->func_decl.first_param;
                    while (param) {
                        accept(param, curr);
                        param = param->next_sibling;
                    }
                }
                accept(curr->func_decl.body, curr);
                if (useScopes) {
                    auto sym = symbolTable.getSymbol(curr->func_decl.symbol);
                    sym->frame_slot_count = currFrame().maxSlots;
                    popFrame();
                    symbolTable.endScope();
                }
            }
            curr = curr->next_sibling;
        }
    }
}

void Analyzer::pushFrame() { _frameStack.emplace(); }

void Analyzer::popFrame() noexcept { _frameStack.pop(); }

void Analyzer::saveFrameSlot(Symbol* sym) noexcept {
    if (hasFrame()) {
        auto& frame = currFrame();
        sym->slot_index = frame.currentSlot++;
        frame.maxSlots = std::max(frame.maxSlots, frame.currentSlot);
    }
}

bool Analyzer::hasFrame() const noexcept { return !_frameStack.empty(); }

Analyzer::FunctionFrameState& Analyzer::currFrame() noexcept { return _frameStack.top(); }

adv_node_t* Analyzer::promoteCastNode(adv_node_t* node, vm_value_type_t targetType) {
    if (!node || node->resolved_type == targetType) {
        return node;
    }
    auto castNode = _ctx.allocNode(node->location, ADV_NODE_IMPLICIT_CAST_EXPR);

    castNode->cast_expr.operand = node;
    castNode->cast_expr.from_type = node->resolved_type;
    castNode->cast_expr.to_type = targetType;
    castNode->resolved_type = targetType;
    return castNode;
}

bool Analyzer::isEqualityOp(adv_uint32_t op) noexcept {
    switch (op) {
    case Parser::token::EQ:
    case Parser::token::NE:
        return true;
    default:
        return false;
    }
}

bool Analyzer::isOrderingOp(adv_uint32_t op) noexcept {
    switch (op) {
    case '<':
    case '>':
    case Parser::token::LE:
    case Parser::token::GE:
        return true;
    default:
        return false;
    }
}

bool Analyzer::isComparisonOp(adv_uint32_t op) noexcept { return isEqualityOp(op) || isOrderingOp(op); }

bool Analyzer::isArithmeticOp(adv_uint32_t op) noexcept {
    switch (op) {
    case '+':
    case '-':
    case '*':
    case '/':
    case '%':
        return true;
    default:
        return false;
    }
}

bool Analyzer::isBitwiseOp(adv_uint32_t op) noexcept {
    switch (op) {
    case '|':
    case '&':
    case '^':
    case Parser::token::ASHL:
    case Parser::token::ASHR:
    case Parser::token::LSHL:
    case Parser::token::LSHR:
        return true;
    default:
        return false;
    }
}

bool Analyzer::isLogicOp(adv_uint32_t op) noexcept {
    switch (op) {
    case Parser::token::AND:
    case Parser::token::OR:
        return true;
    default:
        return false;
    }
}

CastResult Analyzer::getCastResult(vm_value_type_t from, vm_value_type_t to, bool isExplicitAs) noexcept {
    if (from == to) {
        return CastResult::OK;
    }
    if (from == VM_VALUE_TYPE_BOOL) {
        return isExplicitAs ? CastResult::OK : CastResult::ErrorForbidden;
    }
    if (from == VM_VALUE_TYPE_INT && to == VM_VALUE_TYPE_FLOAT) {
        return CastResult::OK;
    }
    if (from == VM_VALUE_TYPE_FLOAT && to == VM_VALUE_TYPE_INT) {
        return isExplicitAs ? CastResult::OK : CastResult::WarningTruncate;
    }
    return isExplicitAs ? CastResult::OK : CastResult::ErrorForbidden;
}

std::string_view Analyzer::getTypeString(vm_value_type_t type) noexcept {
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

vm_value_type_t Analyzer::findBaseType(vm_string_h type) const noexcept {
    return type.handle < VM_VALUE_TYPE_COUNT_ENUMS ? (vm_value_type_t)type.handle : VM_VALUE_TYPE_UNKNOWN;
}

} // namespace adv
