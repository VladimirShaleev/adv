#include "analyzer.hpp"
#include "context.hpp"
#include "options.hpp"
#include "parser.hpp"
#include "visitor.hpp"

using namespace std::literals;

namespace adv {

Analyzer::Analyzer(const Options& options, Context& ctx) noexcept : _options(options), _ctx(ctx) {
    _ctx.symbolTable().init(options, _ctx);
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

    if (_ctx.hasErrors()) {
        return ADV_RESULT_COMPILATION_FAILED;
    }
    resolveSlots();

    return _ctx.hasErrors() ? ADV_RESULT_COMPILATION_FAILED : ADV_RESULT_SUCCESS;
}

void Analyzer::resolveSymbols() {
    struct SymbolVisitor {
        SymbolVisitor(
            const Options& options,
            Context& ctx,
            Analyzer& analyzer,
            std::set<std::string>& modules,
            SymbolTable& symbolTable,
            adv_node_t* parent) noexcept
            : options(options), ctx(ctx), analyzer(analyzer), modules(modules), symbolTable(symbolTable),
              parent(parent) {}

        void visit(adv_node_t* node, Tag<ADV_NODE_PROGRAM>) {
            node->parent = parent;
            visitChildrens(node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_IMPORT_STMT>) {
            node->parent = parent;
            auto importModule = utils::getModuleName(ctx.getString(node->import_stmt.path));
            if (!modules.contains(importModule)) {
                modules.insert(importModule);
                if (node->import_stmt.root) {
                    acceptNode(node->import_stmt.root, node);
                }
                if (ctx.codeFormat(importModule) == utils::Format::Advb) {
                    loadSymbolsFromBinary(ctx.bytecode(importModule));
                }
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_VAR_DECL>) {
            node->parent = parent;
            symbolTable.addSymbol(options, ctx, node);
            acceptNode(node->var_decl.init_expr, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_NATIVE_DECL>) {
            node->parent = parent;
            symbolTable.addSymbol(options, ctx, node);
            auto param = node->func_decl.first_param;
            if (param) {
                symbolTable.beginScope();
                while (param) {
                    acceptNode(param, node);
                    param = param->next_sibling;
                }
                symbolTable.endScope();
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_FUNCTION_DECL>) {
            node->parent = parent;
            symbolTable.addSymbol(options, ctx, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_STRUCT_DECL>) {
            node->parent = parent;
            symbolTable.addSymbol(options, ctx, node);
            if (node->struct_decl.field_count == 0) {
                ctx.log<ADV_STATUS_N1002>(options, node->location, node->struct_decl.name);
            }
            symbolTable.beginScope();
            auto field = node->struct_decl.first_field;
            while (field) {
                acceptNode(field, node);
                field = field->next_sibling;
            }
            symbolTable.endScope();
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_FIELD_DECL>) {
            node->parent = parent;
            symbolTable.addSymbol(options, ctx, node);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_PARAM_DECL>) {
            node->parent = parent;
            symbolTable.addSymbol(options, ctx, node);
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
            auto [handle, _] = symbolTable.lookup(options, ctx, node->location, node->call_expr.name);
            node->parent = parent;
            node->call_expr.symbol = handle;
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
            const auto isNonFunctionBlock = parent->type != ADV_NODE_FUNCTION_DECL;
            if (isNonFunctionBlock) {
                symbolTable.beginScope();
            }
            visitChildrens(node);
            if (isNonFunctionBlock) {
                symbolTable.endScope();
            }
        }
        void visitChildrens(adv_node_t* node) {
            analyzer.visitStmtsWithRules(node, [this](auto node, auto parent) { acceptNode(node, parent); }, true);
        }
        void acceptNode(adv_node_t* node, adv_node_t* parent) {
            accept<SymbolVisitor>(
                node,
                std::cref(options),
                std::ref(ctx),
                std::ref(analyzer),
                std::ref(modules),
                std::ref(symbolTable),
                parent);
        }
        void loadSymbolsFromBinary(std::span<const uint8_t> bytecode) noexcept {
            const auto data = utils::read<utils::SectionData>(bytecode).data;
            const auto exports = utils::read<vm_script_export_t>(bytecode);
            const auto moduleName = ctx.intern(utils::readString(exports.module_name, data));
            const auto location = adv_location_t{moduleName, 0, 0};
            auto resolveExportType = [&](vm_uint32_t type) -> adv_symbol_h {
                const auto baseType = utils::exportTypeBase(type);
                if (baseType > VM_VALUE_TYPE_UNKNOWN && baseType <= VM_VALUE_TYPE_STRING) {
                    return symbolTable.lookup(static_cast<vm_value_type_t>(baseType));
                }
                if (baseType == utils::ExportTypeStructure) {
                    const auto typeNameOffset = utils::exportTypeDataOffset(type);
                    const auto typeName = ctx.intern(utils::readString(typeNameOffset, data));
                    return symbolTable.lookup(symbolTable.getGlobalScope(), typeName);
                }
                return Undefined;
            };
            std::vector<adv_node_t> structures;
            structures.reserve(exports.struct_count);
            for (adv_uint32_t i = 0; i < exports.struct_count; ++i) {
                const auto exported = utils::read<vm_script_export_struct_t>(bytecode, i);
                auto& node = structures.emplace_back(ADV_NODE_STRUCT_DECL, location);
                node.struct_decl.name = ctx.intern(utils::readString(exported.name, data));
                symbolTable.addSymbol(options, ctx, &node);
            }
            for (adv_uint32_t i = 0; i < exports.struct_count; ++i) {
                const auto exported = utils::read<vm_script_export_struct_t>(bytecode, i);
                symbolTable.beginScope();
                for (adv_uint32_t j = 0; j < exported.field_count; ++j) {
                    const auto field = utils::read<vm_script_export_field_t>(bytecode, i, j);
                    adv_node_t fieldNode{ADV_NODE_FIELD_DECL, location, Undefined, &structures[i]};
                    fieldNode.field_decl.name = ctx.intern(utils::readString(field.name, data));
                    fieldNode.field_decl.symbol = Undefined;
                    auto fieldSymbol = symbolTable.addSymbol(options, ctx, &fieldNode);
                    fieldSymbol->type = resolveExportType(field.type);
                }
                symbolTable.endScope();
            }
            for (adv_uint32_t i = 0; i < exports.var_count; ++i) {
                const auto var = utils::read<vm_script_export_var_t>(bytecode, i);
                adv_node_t node{ADV_NODE_VAR_DECL, location};
                node.var_decl.name = ctx.intern(utils::readString(var.name, data));
                node.var_decl.is_global = 1;
                node.resolved_type = resolveExportType(var.type);
                if (node.resolved_type != Undefined) {
                    node.var_decl.type_hint = symbolTable.getSymbol(node.resolved_type)->name;
                    auto sym = symbolTable.addSymbol(options, ctx, &node);
                    sym->type = node.resolved_type;
                }
            }
            std::vector<adv_node_t> params;
            params.reserve(16);
            for (adv_uint32_t i = 0; i < exports.func_count; ++i) {
                params.clear();
                const auto func = utils::read<vm_script_export_func_t>(bytecode, i);
                adv_node_t node{ADV_NODE_FUNCTION_DECL, location};
                for (adv_uint32_t j = 0; j < func.param_count; ++j) {
                    const auto param = utils::read<vm_script_export_param_t>(bytecode, i, j);
                    auto& nodeParam = params.emplace_back(ADV_NODE_PARAM_DECL, location, Undefined, &node);
                    nodeParam.param_decl.name = ctx.intern(utils::readString(param.name, data));
                    nodeParam.resolved_type = resolveExportType(param.type);
                    if (nodeParam.resolved_type != Undefined) {
                        nodeParam.param_decl.type_hint = symbolTable.getSymbol(nodeParam.resolved_type)->name;
                    }
                }
                node.func_decl.name = ctx.intern(utils::readString(func.name, data));
                node.func_decl.param_count = static_cast<adv_uint32_t>(params.size());
                node.resolved_type = func.return_type == VM_VALUE_TYPE_UNKNOWN ? symbolTable.lookupVoid()
                                                                               : resolveExportType(func.return_type);
                if (!params.empty()) {
                    node.func_decl.first_param = &params.front();
                    auto prevIt = params.begin();
                    for (auto it = params.begin() + 1; it != params.end(); ++it) {
                        prevIt->next_sibling = &(*it);
                        prevIt = it;
                    }
                }
                symbolTable.addSymbol(options, ctx, &node)->type = node.resolved_type;
                symbolTable.beginScope();
                for (auto& param : params) {
                    auto sym = symbolTable.addSymbol(options, ctx, &param);
                    sym->type = param.resolved_type;
                }
                symbolTable.endScope();
            }
            for (adv_uint32_t i = 0; i < exports.native_count; ++i) {
                params.clear();
                const auto native = utils::read<vm_script_export_native_t>(bytecode, i);
                adv_node_t node{ADV_NODE_NATIVE_DECL, location};
                for (adv_uint32_t j = 0; j < native.param_count; ++j) {
                    const auto param = utils::read<utils::NativeParam>(bytecode, i, j).param;
                    auto& nodeParam = params.emplace_back(ADV_NODE_PARAM_DECL, location, Undefined, &node);
                    nodeParam.param_decl.name = ctx.intern(utils::readString(param.name, data));
                    nodeParam.resolved_type = resolveExportType(param.type);
                    if (nodeParam.resolved_type != Undefined) {
                        nodeParam.param_decl.type_hint = symbolTable.getSymbol(nodeParam.resolved_type)->name;
                    }
                }
                node.func_decl.name = ctx.intern(utils::readString(native.name, data));
                node.func_decl.param_count = static_cast<adv_uint32_t>(params.size());
                node.resolved_type = native.return_type == VM_VALUE_TYPE_UNKNOWN
                                         ? symbolTable.lookupVoid()
                                         : resolveExportType(native.return_type);
                if (!params.empty()) {
                    node.func_decl.first_param = &params.front();
                    auto prevIt = params.begin();
                    for (auto it = params.begin() + 1; it != params.end(); ++it) {
                        prevIt->next_sibling = &(*it);
                        prevIt = it;
                    }
                }
                symbolTable.addSymbol(options, ctx, &node)->type = node.resolved_type;
                symbolTable.beginScope();
                for (auto& param : params) {
                    auto sym = symbolTable.addSymbol(options, ctx, &param);
                    sym->type = param.resolved_type;
                }
                symbolTable.endScope();
            }
        }

        const Options& options;
        Context& ctx;
        Analyzer& analyzer;
        std::set<std::string>& modules;
        SymbolTable& symbolTable;
        adv_node_t* parent;
    };
    std::set<std::string> modules;
    accept<SymbolVisitor>(
        _ctx.root(),
        std::cref(_options),
        std::ref(_ctx),
        std::ref(*this),
        std::ref(modules),
        std::ref(_ctx.symbolTable()),
        nullptr);
}

void Analyzer::resolveTypes() {
    struct TypeVisitor {
        TypeVisitor(const Options& options, Context& ctx, Analyzer& analyzer, SymbolTable& symbolTable) noexcept
            : options(options), ctx(ctx), analyzer(analyzer), symbolTable(symbolTable) {}
        void visit(adv_node_t* node, Tag<ADV_NODE_PROGRAM>) { visitChildrens(node); }
        void visit(adv_node_t* node, Tag<ADV_NODE_IMPORT_STMT>) { acceptNode(node->import_stmt.root); }
        void visit(adv_node_t* node, Tag<ADV_NODE_STRUCT_DECL>) {
            auto field = node->struct_decl.first_field;
            while (field) {
                acceptNode(field);
                field = field->next_sibling;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_FIELD_DECL>) {
            auto field = symbolTable.getSymbol(node->field_decl.symbol);
            const auto fieldType = analyzer.findBaseType(field->scope, node->field_decl.type_hint);
            if (fieldType == Undefined) {
                ctx.log<ADV_STATUS_E3048>(options, node->location, node->field_decl.type_hint, node->field_decl.name);
                return;
            }
            field->type = fieldType;
            node->resolved_type = fieldType;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_INT_LITERAL>) {
            node->resolved_type = symbolTable.lookup(VM_VALUE_TYPE_INT);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_FLOAT_LITERAL>) {
            node->resolved_type = symbolTable.lookup(VM_VALUE_TYPE_FLOAT);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_STRING_LITERAL>) {
            node->resolved_type = symbolTable.lookup(VM_VALUE_TYPE_STRING);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_BOOL_LITERAL>) {
            node->resolved_type = symbolTable.lookup(VM_VALUE_TYPE_BOOL);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_NATIVE_DECL>) {
            auto returnType = symbolTable.lookupVoid();
            if (node->func_decl.return_type_hint.handle != 0) {
                auto scope = symbolTable.getSymbol(node->func_decl.symbol)->scope;
                returnType = analyzer.findBaseType(scope, node->func_decl.return_type_hint);
                if (returnType == Undefined) {
                    ctx.log<ADV_STATUS_E3045>(
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
        void visit(adv_node_t* node, Tag<ADV_NODE_FUNCTION_DECL>) {
            auto returnType = symbolTable.lookupVoid();
            if (node->func_decl.return_type_hint.handle != 0) {
                auto scope = symbolTable.getSymbol(node->func_decl.symbol)->scope;
                returnType = analyzer.findBaseType(scope, node->func_decl.return_type_hint);
                if (returnType == Undefined) {
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
            adv_node_t* fn = node->parent;
            while (fn && fn->type != ADV_NODE_FUNCTION_DECL) {
                fn = fn->parent;
            }
            if (!fn) {
                ctx.log<ADV_STATUS_E3035>(options, node->location);
                return;
            }
            const bool hasExpr = node->expr_stmt.expr != nullptr;
            adv_symbol_h retType = Undefined;
            if (hasExpr) {
                retType = acceptNode(node->expr_stmt.expr);
                if (retType == Undefined) {
                    return;
                }
            }
            const auto resolvedType = symbolTable.getSymbol(fn->resolved_type);
            if (resolvedType->kind == ADV_SYMBOL_KIND_VOID && hasExpr) {
                ctx.log<ADV_STATUS_E3013>(options, node->location, fn->func_decl.name);
                return;
            }
            if (resolvedType->kind != ADV_SYMBOL_KIND_VOID && !hasExpr) {
                ctx.log<ADV_STATUS_E3014>(options, node->location, fn->func_decl.name);
                return;
            }
            if (fn->resolved_type != retType) {
                auto result = analyzer.getCastResult(retType, fn->resolved_type, false);
                if (result == CastResult::WarningTruncate) {
                    ctx.log<ADV_STATUS_W2002>(options, node->location, retType, fn->func_decl.return_type_hint);
                } else if (result == CastResult::ErrorForbidden) {
                    ctx.log<ADV_STATUS_E3015>(
                        options, node->location, retType, fn->func_decl.return_type_hint, fn->func_decl.name);
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
            auto scope = symbolTable.getSymbol(node->param_decl.symbol)->scope;
            const auto paramType = analyzer.findBaseType(scope, node->param_decl.type_hint);
            if (paramType == Undefined) {
                ctx.log<ADV_STATUS_E3017>(options, node->location, node->param_decl.type_hint, node->param_decl.name);
                return;
            }
            symbolTable.getSymbol(node->param_decl.symbol)->type = paramType;
            node->resolved_type = paramType;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_IDENTIFIER_EXPR>) {
            const auto sym = symbolTable.getSymbol(node->id_expr.symbol);
            if (sym) {
                if (sym->type == Undefined) {
                    ctx.log<ADV_STATUS_E3018>(options, node->location, node->id_expr.name);
                    return;
                }
                node->resolved_type = sym->type;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_BINARY_EXPR>) {
            const auto leftType = acceptNode(node->binary_expr.left);
            const auto rightType = acceptNode(node->binary_expr.right);
            if (leftType == Undefined || rightType == Undefined) {
                return;
            }
            const auto leftSymbol = symbolTable.getSymbol(leftType);
            const auto rightSymbol = symbolTable.getSymbol(rightType);
            if (leftSymbol->kind != ADV_SYMBOL_KIND_PRIMITIVE || rightSymbol->kind != ADV_SYMBOL_KIND_PRIMITIVE) {
                // TODO: err
                return;
            }
            const auto leftPrimitive = leftSymbol->primitive_type;
            const auto rightPrimitive = rightSymbol->primitive_type;
            const auto op = node->binary_expr.op;

            if (isLogicOp(op)) {
                if (leftPrimitive != VM_VALUE_TYPE_BOOL || rightPrimitive != VM_VALUE_TYPE_BOOL) {
                    ctx.log<ADV_STATUS_E3031>(options, node->location, leftSymbol->name, rightSymbol->name);
                    return;
                }
                node->resolved_type = symbolTable.lookup(VM_VALUE_TYPE_BOOL);
                return;
            }

            if (isBitwiseOp(op) || op == '%') {
                if (leftPrimitive != VM_VALUE_TYPE_INT || rightPrimitive != VM_VALUE_TYPE_INT) {
                    ctx.log<ADV_STATUS_E3032>(options, node->location, leftSymbol->name, rightSymbol->name);
                    return;
                }
                node->resolved_type = symbolTable.lookup(VM_VALUE_TYPE_INT);
                return;
            }

            if (isOrderingOp(op)) {
                if (leftPrimitive == VM_VALUE_TYPE_BOOL || rightPrimitive == VM_VALUE_TYPE_BOOL) {
                    ctx.log<ADV_STATUS_E3033>(options, node->location);
                    return;
                }
            }

            auto targetType = leftType;
            if (leftType != rightType) {
                const auto needSwap = static_cast<int>(rightPrimitive) < static_cast<int>(leftPrimitive);
                const auto from = needSwap ? rightType : leftType;
                const auto to = needSwap ? leftType : rightType;
                const auto result = analyzer.getCastResult(from, to, false);
                if (result == CastResult::WarningTruncate) {
                    ctx.log<ADV_STATUS_W2003>(options, node->location, from, to);
                } else if (result == CastResult::ErrorForbidden) {
                    ctx.log<ADV_STATUS_E3019>(options, node->location, from, to);
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
                node->resolved_type = symbolTable.lookup(VM_VALUE_TYPE_BOOL);
            } else {
                node->resolved_type = targetType;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_CALL_EXPR>) {
            auto sym = symbolTable.getSymbol(node->call_expr.symbol);
            if (sym->kind != ADV_SYMBOL_KIND_FUNCTION && sym->kind != ADV_SYMBOL_KIND_NATIVE) {
                ctx.log<ADV_STATUS_E3021>(options, node->location, node->call_expr.name);
                return;
            }
            node->resolved_type = sym->type;

            adv_node_t** argPtr = &node->call_expr.first_arg;
            const auto& params = sym->linearChildren;
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
                            options, arg->location, argType, paramType, param->name, node->call_expr.name);
                    } else if (result == CastResult::ErrorForbidden) {
                        ctx.log<ADV_STATUS_E3023>(
                            options, arg->location, param->name, argType, paramType, node->call_expr.name);
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
            auto typeHandle = acceptNode(node->unary_expr.operand);
            auto typeSymbol = symbolTable.getSymbol(typeHandle);
            assert(typeSymbol);
            if (typeSymbol->kind != ADV_SYMBOL_KIND_PRIMITIVE) {
                // TODO: err
                return;
            }
            const auto type = typeSymbol->primitive_type;
            const auto op = node->unary_expr.op;
            if (op == Parser::token::NOT) {
                if (type != VM_VALUE_TYPE_BOOL) {
                    ctx.log<ADV_STATUS_E3028>(options, node->location, typeSymbol->name);
                    return;
                }
            } else if (op == '~') {
                if (type != VM_VALUE_TYPE_INT) {
                    ctx.log<ADV_STATUS_E3029>(options, node->location, typeSymbol->name);
                    return;
                }
            } else if (op == '-') {
                if (type != VM_VALUE_TYPE_INT && type != VM_VALUE_TYPE_FLOAT) {
                    ctx.log<ADV_STATUS_E3030>(options, node->location, typeSymbol->name);
                    return;
                }
            } else {
                assert(!"unreachable code");
            }
            node->resolved_type = typeHandle;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_ASSIGN_EXPR>) {
            const auto targetType = acceptNode(node->assign_expr.target);
            if (targetType == Undefined) {
                return;
            }
            const auto valueType = acceptNode(node->assign_expr.val);
            if (valueType == Undefined) {
                return;
            }
            if (targetType != valueType) {
                auto result = analyzer.getCastResult(valueType, targetType, false);
                if (result == CastResult::WarningTruncate) {
                    ctx.log<ADV_STATUS_W2005>(options, node->location, valueType, targetType);
                } else if (result == CastResult::ErrorForbidden) {
                    ctx.log<ADV_STATUS_E3024>(options, node->location, valueType, targetType);
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
            adv_symbol_h hintType = Undefined;
            adv_symbol_h initType = Undefined;
            if (hasTypeHint) {
                auto scope = symbolTable.getSymbol(node->var_decl.symbol)->scope;
                hintType = analyzer.findBaseType(scope, node->var_decl.type_hint);
                if (hintType == Undefined) {
                    ctx.log<ADV_STATUS_E3026>(options, node->location, node->var_decl.type_hint, node->var_decl.name);
                    return;
                }
            }
            if (hasInitExpr) {
                initType = acceptNode(node->var_decl.init_expr);
                if (initType == Undefined) {
                    return;
                }
            }
            if (hasTypeHint && hasInitExpr && hintType != initType) {
                auto result = analyzer.getCastResult(initType, hintType, false);
                if (result == CastResult::WarningTruncate) {
                    ctx.log<ADV_STATUS_W2006>(options, node->location, initType, hintType, node->var_decl.name);
                } else if (result == CastResult::ErrorForbidden) {
                    ctx.log<ADV_STATUS_E3027>(options, node->location, node->var_decl.name, initType, hintType);
                    return;
                }
                node->var_decl.init_expr = analyzer.promoteCastNode(node->var_decl.init_expr, hintType);
            }
            const auto finalType = hasTypeHint ? hintType : initType;

            symbolTable.getSymbol(node->var_decl.symbol)->type = finalType;
            node->resolved_type = finalType;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_EXPLICIT_CAST_EXPR>) {
            const auto srcType = acceptNode(node->cast_expr.operand);
            if (srcType == Undefined) {
                return;
            }
            adv_node_t* fn = node->parent;
            while (fn && fn->type != ADV_NODE_FUNCTION_DECL) {
                fn = fn->parent;
            }
            auto scope = fn ? symbolTable.getSymbol(fn->func_decl.symbol)->scope : symbolTable.getGlobalScope();
            const auto targetType = analyzer.findBaseType(scope, node->cast_expr.target_type_hint);
            if (targetType == Undefined) {
                ctx.log<ADV_STATUS_E3010>(options, node->location, node->cast_expr.target_type_hint);
                return;
            }
            const auto result = analyzer.getCastResult(srcType, targetType, true);
            if (result == CastResult::ErrorForbidden) {
                ctx.log<ADV_STATUS_E3012>(options, node->location, srcType, node->cast_expr.target_type_hint);
                return;
            }
            node->cast_expr.from_type = srcType;
            node->cast_expr.to_type = targetType;
            node->resolved_type = targetType;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_BLOCK>) { visitChildrens(node); }
        void visit(adv_node_t* node, Tag<ADV_NODE_IF_STMT>) {
            const auto condType = acceptNode(node->if_stmt.cond);
            if (condType != Undefined && condType != symbolTable.lookup(VM_VALUE_TYPE_BOOL)) {
                ctx.log<ADV_STATUS_E3034>(options, node->if_stmt.cond->location, condType);
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
            if (condType != Undefined && condType != symbolTable.lookup(VM_VALUE_TYPE_BOOL)) {
                ctx.log<ADV_STATUS_E3036>(options, node->while_stmt.cond->location, condType);
            }
            if (node->while_stmt.body) {
                acceptNode(node->while_stmt.body);
            }
        }
        void visit(adv_node_t* /* node */, Tag<ADV_NODE_YIELD_STMT>) { assert(!"not implemented"); }
        void visit(adv_node_t* node, Tag<ADV_NODE_WAIT_STMT>) {
            if (!node->expr_stmt.expr) {
                ctx.log<ADV_STATUS_E3041>(options, node->location);
                return;
            }
            const auto exprType = acceptNode(node->expr_stmt.expr);
            if (exprType == Undefined) {
                return;
            }
            if (exprType != symbolTable.lookup(VM_VALUE_TYPE_INT) &&
                exprType != symbolTable.lookup(VM_VALUE_TYPE_FLOAT)) {
                ctx.log<ADV_STATUS_E3040>(options, node->expr_stmt.expr->location, exprType);
                return;
            }
        }
        void visit(adv_node_t* /* node */, Tag<ADV_NODE_IMPLICIT_CAST_EXPR>) { assert(!"unreachable code"); }
        void visit(adv_node_t* node, Tag<ADV_NODE_MEMBER_ACCESS>) {
            const auto targetTypeHandle = acceptNode(node->member_access.target);
            if (targetTypeHandle == Undefined) {
                return;
            }
            const auto targetType = symbolTable.getSymbol(targetTypeHandle);
            if (!targetType) {
                return;
            }
            if (targetType->kind == ADV_SYMBOL_KIND_STRUCTURE) {
                for (const auto fieldHandle : targetType->linearChildren) {
                    const auto field = symbolTable.getSymbol(fieldHandle);
                    if (field && field->name == node->member_access.member) {
                        if (field->type == Undefined) {
                            ctx.log<ADV_STATUS_E3018>(options, node->location, field->name);
                            return;
                        }
                        node->member_access.symbol = fieldHandle;
                        node->resolved_type = field->type;
                        return;
                    }
                }
            }
            ctx.log<ADV_STATUS_E3049>(options, node->location, targetTypeHandle, node->member_access.member);
        }
        void visitChildrens(adv_node_t* node) {
            analyzer.visitStmtsWithRules(node, [this](auto node, auto /* parent */) { acceptNode(node); });
        }
        adv_symbol_h acceptNode(adv_node_t* node) {
            accept<TypeVisitor>(node, std::cref(options), std::ref(ctx), std::ref(analyzer), std::ref(symbolTable));
            return node ? node->resolved_type : Undefined;
        }
        const Options& options;
        Context& ctx;
        Analyzer& analyzer;
        SymbolTable& symbolTable;
    };
    accept<TypeVisitor>(
        _ctx.root(), std::cref(_options), std::ref(_ctx), std::ref(*this), std::ref(_ctx.symbolTable()));
}

void Analyzer::resolveSlots() {
    struct SlotVisitor {
        SlotVisitor(const Options& options, Context& ctx, Analyzer& analyzer, SymbolTable& symbolTable) noexcept
            : options(options), ctx(ctx), analyzer(analyzer), symbolTable(symbolTable) {}

        void visit(adv_node_t* node, Tag<ADV_NODE_PROGRAM>) { visitChildrens(node); }
        void visit(adv_node_t* node, Tag<ADV_NODE_IMPORT_STMT>) { acceptNode(node->import_stmt.root); }
        void visit(adv_node_t* node, Tag<ADV_NODE_STRUCT_DECL>) {
            calculateStructureSlots(symbolTable.getSymbol(node->struct_decl.symbol), nullptr);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_NATIVE_DECL>) {
            auto param = node->func_decl.first_param;
            while (param) {
                calculateSymbolSlots(symbolTable.getSymbol(param->param_decl.symbol));
                param = param->next_sibling;
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_VAR_DECL>) {
            auto sym = symbolTable.getSymbol(node->var_decl.symbol);
            if (calculateSymbolSlots(sym)) {
                analyzer.saveFrameSlot(sym);
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_PARAM_DECL>) {
            auto sym = symbolTable.getSymbol(node->param_decl.symbol);
            if (calculateSymbolSlots(sym)) {
                analyzer.saveFrameSlot(sym);
            }
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_BLOCK>) {
            const auto slotsBeforeBlock = analyzer.currFrame().currentSlot;
            visitChildrens(node);
            analyzer.currFrame().currentSlot = slotsBeforeBlock;
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_IF_STMT>) {
            acceptNode(node->if_stmt.cond);
            acceptNode(node->if_stmt.then_branch);
            acceptNode(node->if_stmt.else_branch);
        }
        void visit(adv_node_t* node, Tag<ADV_NODE_WHILE_STMT>) {
            acceptNode(node->while_stmt.cond);
            acceptNode(node->while_stmt.body);
        }
        void visit(...) {}
        void visitChildrens(adv_node_t* node) {
            analyzer.visitStmtsWithRules(
                node, [this](auto child, auto /* parent */) { acceptNode(child); }, false, true);
        }
        void acceptNode(adv_node_t* node) {
            accept<SlotVisitor>(node, std::cref(options), std::ref(ctx), std::ref(analyzer), std::ref(symbolTable));
        }
        bool calculateSymbolSlots(Symbol* sym) {
            const auto type = symbolTable.getSymbol(sym->type);
            if (!type) {
                return false;
            }
            if (type->kind == ADV_SYMBOL_KIND_PRIMITIVE) {
                sym->slot_count = 1;
                return true;
            }
            if (type->kind == ADV_SYMBOL_KIND_STRUCTURE) {
                if (!calculateStructureSlots(type, nullptr)) {
                    return false;
                }
                sym->slot_count = type->slot_count;
                return true;
            }
            return false;
        }
        bool calculateStructureSlots(Symbol* structure, const Symbol* throughField) {
            switch (structure->slotLayoutState) {
            case SlotLayoutState::Resolved:
                return true;
            case SlotLayoutState::Invalid:
                return false;
            case SlotLayoutState::Resolving:
                assert(throughField);
                ctx.log<ADV_STATUS_E3046>(options, throughField->location, structure->name, throughField->name);
                return false;
            case SlotLayoutState::Unresolved:
                break;
            }

            structure->slotLayoutState = SlotLayoutState::Resolving;
            adv_uint32_t slotCount = 0;
            const auto maxSlotCount = static_cast<adv_uint32_t>(std::numeric_limits<adv_uint16_t>::max());
            for (const auto fieldHandle : structure->linearChildren) {
                auto field = symbolTable.getSymbol(fieldHandle);
                auto fieldType = symbolTable.getSymbol(field->type);
                if (!fieldType) {
                    structure->slotLayoutState = SlotLayoutState::Invalid;
                    return false;
                }

                adv_uint32_t fieldSlotCount = 0;
                if (fieldType->kind == ADV_SYMBOL_KIND_PRIMITIVE) {
                    fieldSlotCount = 1;
                } else if (fieldType->kind == ADV_SYMBOL_KIND_STRUCTURE) {
                    if (!calculateStructureSlots(fieldType, field)) {
                        structure->slotLayoutState = SlotLayoutState::Invalid;
                        return false;
                    }
                    fieldSlotCount = fieldType->slot_count;
                } else {
                    structure->slotLayoutState = SlotLayoutState::Invalid;
                    return false;
                }

                if (fieldSlotCount > maxSlotCount - slotCount) {
                    ctx.log<ADV_STATUS_E3047>(options, field->location, field->name);
                    structure->slotLayoutState = SlotLayoutState::Invalid;
                    return false;
                }
                field->slot_index = static_cast<adv_uint16_t>(slotCount);
                field->slot_count = static_cast<adv_uint16_t>(fieldSlotCount);
                slotCount += fieldSlotCount;
            }

            structure->slot_count = static_cast<adv_uint16_t>(slotCount);
            structure->slotLayoutState = SlotLayoutState::Resolved;
            return true;
        }

        const Options& options;
        Context& ctx;
        Analyzer& analyzer;
        SymbolTable& symbolTable;
    };

    pushFrame();
    accept<SlotVisitor>(
        _ctx.root(), std::cref(_options), std::ref(_ctx), std::ref(*this), std::ref(_ctx.symbolTable()));
    _ctx.setRootFrameSlots(static_cast<adv_uint16_t>(currFrame().maxSlots));
    popFrame();
}

void Analyzer::visitStmtsWithRules(
    adv_node_t* node, std::function<void(adv_node_t*, adv_node_t*)> accept, bool useScopes, bool allocateSlots) {
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
                }
                if (allocateSlots) {
                    pushFrame();
                }
                if (useScopes || allocateSlots) {
                    auto param = curr->func_decl.first_param;
                    while (param) {
                        accept(param, curr);
                        param = param->next_sibling;
                    }
                }
                accept(curr->func_decl.body, curr);
                if (allocateSlots) {
                    auto sym = symbolTable.getSymbol(curr->func_decl.symbol);
                    sym->slot_count = currFrame().maxSlots;
                    popFrame();
                }
                if (useScopes) {
                    symbolTable.endScope();
                }
            }
            curr = curr->next_sibling;
        }
    }
}

void Analyzer::pushFrame() { _frameStack.emplace(); }

void Analyzer::popFrame() noexcept { _frameStack.pop(); }

void Analyzer::saveFrameSlot(Symbol* sym) {
    if (!sym->is_global) {
        auto& frame = currFrame();
        const auto nextSlot = frame.currentSlot + sym->slot_count;
        const auto maxSlotCount = static_cast<adv_uint32_t>(std::numeric_limits<adv_uint16_t>::max());
        if (nextSlot > maxSlotCount) {
            _ctx.log<ADV_STATUS_E3047>(_options, sym->location, sym->name);
            return;
        }
        sym->slot_index = static_cast<adv_uint16_t>(frame.currentSlot);
        frame.currentSlot = nextSlot;
        frame.maxSlots = std::max(frame.maxSlots, nextSlot);
    }
}

Analyzer::FunctionFrameState& Analyzer::currFrame() noexcept { return _frameStack.top(); }

adv_node_t* Analyzer::promoteCastNode(adv_node_t* node, adv_symbol_h targetType) {
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

CastResult Analyzer::getCastResult(adv_symbol_h from, adv_symbol_h to, bool isExplicitAs) noexcept {
    if (from == to) {
        return CastResult::OK;
    }
    const auto symbolFrom = _ctx.symbolTable().getSymbol(from);
    const auto symbolTo = _ctx.symbolTable().getSymbol(to);
    assert(symbolFrom);
    assert(symbolTo);
    if (symbolFrom->kind != ADV_SYMBOL_KIND_PRIMITIVE || symbolTo->kind != ADV_SYMBOL_KIND_PRIMITIVE) {
        return CastResult::ErrorForbidden;
    }
    if (symbolFrom->primitive_type == VM_VALUE_TYPE_BOOL) {
        return isExplicitAs ? CastResult::OK : CastResult::ErrorForbidden;
    }
    if (symbolFrom->primitive_type == VM_VALUE_TYPE_INT && symbolTo->primitive_type == VM_VALUE_TYPE_FLOAT) {
        return CastResult::OK;
    }
    if (symbolFrom->primitive_type == VM_VALUE_TYPE_FLOAT && symbolTo->primitive_type == VM_VALUE_TYPE_INT) {
        return isExplicitAs ? CastResult::OK : CastResult::WarningTruncate;
    }
    return isExplicitAs ? CastResult::OK : CastResult::ErrorForbidden;
}

adv_symbol_h Analyzer::findBaseType(adv_scope_h scope, vm_string_h type) const noexcept {
    if (auto typeSymbol = _ctx.symbolTable().lookup(scope, type); typeSymbol != Undefined) {
        auto sym = _ctx.symbolTable().getSymbol(typeSymbol);
        if (sym->kind == ADV_SYMBOL_KIND_PRIMITIVE || sym->kind == ADV_SYMBOL_KIND_STRUCTURE) {
            return typeSymbol;
        }
    }
    return Undefined;
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

} // namespace adv
