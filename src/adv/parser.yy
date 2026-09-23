%require "3.7.4"
%language "C++"
%defines "parser.hpp"
%output "parser.cpp"
%locations

%{
#include "context.hpp"
#include <FlexLexer.h>
%}

%define api.parser.class {Parser}
%define api.namespace {adv}
%define api.value.type variant
%parse-param {Scanner& scanner}

%code requires
{
#include "context.hpp"
namespace adv {
    class Scanner;
    struct NodeList {
        adv_node_t* first{};
        adv_node_t* last{};
        uint32_t count{};
        static NodeList init(adv_node_t* node) noexcept;
        NodeList add(Scanner& scanner, adv_node_t* node) const noexcept;
    };
}
}

%code
{
#include "scanner.hpp"
#define yylex scanner.yylex
#define intern(str) \
    scanner.context().intern({(str).c_str(), (str).length()})
#define ast_location(loc) adv_location_t { \
    intern(*loc.begin.filename), \
    uint32_t(loc.begin.line), \
    uint32_t(loc.begin.column) \
}
#define alloc_node(loc, type) \
    scanner.context().allocNode(ast_location(loc), ADV_NODE_##type)
#define log(status, loc, ...) \
    scanner.log<ADV_STATUS_##status>(ast_location(loc) __VA_OPT__(,) __VA_ARGS__)
adv::NodeList adv::NodeList::init(adv_node_t* node) noexcept { return { node, node, node ? 1u : 0u }; }
adv::NodeList adv::NodeList::add(Scanner& scanner, adv_node_t* node) const noexcept { 
    if (!node) return *this;
    if (!first) {
        return { node, node, 1u };
    }
    last->next_sibling = node; 
    return { first, node, count + 1 };
}
#define list_init(node) NodeList::init(node)
#define list_add(list, node) list.add(scanner, node)
using namespace std::literals;
}

%initial-action {
    @$.begin.filename = @$.end.filename = scanner.filename();
}

/* Keywords */
%token IMPORT FUNCTION VAR AS IF ELSE WHILE YIELD WAIT RETURN

/* Operators */
%token EQ NE LE GE

/* Literals and identifiers */
%token <int32_t>     INT_LITERAL
%token <bool>        BOOL_LITERAL
%token <float>       FLOAT_LITERAL
%token <std::string> STRING_LITERAL IDENTIFIER

/* Non-terminals */
%type <adv_node_t*>  program
%type <NodeList>     statement_list
%type <adv_node_t*>  statement
%type <adv_node_t*>  var_decl
%type <adv_node_t*>  expr
%type <adv_node_t*>  assign_expr
%type <adv_node_t*>  call_expr
%type <adv_node_t*>  if_stmt
%type <adv_node_t*>  while_stmt
%type <adv_node_t*>  import_stmt
%type <adv_node_t*>  return_stmt
%type <adv_node_t*>  function_decl
%type <NodeList>     param_list
%type <NodeList>     arg_list
%type <adv_node_t*>  block

/* Operator priorities */
%right '='
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/'
%left AS
%nonassoc UMINUS
%left '.' '[' ']' '(' ')'

%start program

%%

program
    : statement_list {
        auto prog = alloc_node(@$, PROGRAM);
        prog->children.first_child = $1.first;
        prog->children.child_count = $1.count;
        scanner.context().setNodeTree(*scanner.filename(), prog);
    }
    ;

statement_list
    : statement { $$ = list_init($1); }
    | statement_list statement { $$ = list_add($1, $2); }
    ;

arg_list:
    /* empty */ { $$ = list_init(nullptr); }
    | expr { $$ = list_init($1); }
    | arg_list ',' expr { $$ = list_add($1, $3); }
    ;

param_list:
    /* empty */ { $$ = list_init(nullptr); }
    | IDENTIFIER { 
        log(E3006, @1, $1);
        auto param = alloc_node(@1, PARAM_DECL);
        param->param_decl.name = intern($1);
        param->param_decl.type_hint = intern("<error>"s);
        $$ = list_init(param);
    }
    | IDENTIFIER ':' IDENTIFIER { 
        auto param = alloc_node(@1, PARAM_DECL);
        param->param_decl.name = intern($1);
        param->param_decl.type_hint = intern($3);
        $$ = list_init(param); 
    }
    | param_list ',' IDENTIFIER {
        log(E3006, @3, $3);
        auto param = alloc_node(@3, PARAM_DECL);
        param->param_decl.name = intern($3);
        param->param_decl.type_hint = intern("<error>"s);
        $$ = list_add($1, param);
    }
    | param_list ',' IDENTIFIER ':' IDENTIFIER {
        auto param = alloc_node(@3, PARAM_DECL);
        param->param_decl.name = intern($3);
        param->param_decl.type_hint = intern($5);
        $$ = list_add($1, param);
    }
    ;

var_decl
    : VAR IDENTIFIER {
        log(E3007, @2, $2);
        auto node = alloc_node(@$, VAR_DECL);
        node->var_decl.name = intern($2);
        node->var_decl.type_hint = {0};
        node->var_decl.init_expr = nullptr;
        $$ = node;
    }
    | VAR IDENTIFIER ':' IDENTIFIER {
        auto node = alloc_node(@$, VAR_DECL);
        node->var_decl.name = intern($2);
        node->var_decl.type_hint = intern($4);
        node->var_decl.init_expr = nullptr;
        $$ = node;
    }
    | VAR IDENTIFIER '=' expr {
        auto node = alloc_node(@$, VAR_DECL);
        node->var_decl.name = intern($2);
        node->var_decl.type_hint = {0};
        node->var_decl.init_expr = $4;
        $$ = node;
    }
    | VAR IDENTIFIER ':' IDENTIFIER '=' expr {
        auto node = alloc_node(@$, VAR_DECL);
        node->var_decl.name = intern($2);
        node->var_decl.type_hint = intern($4);
        node->var_decl.init_expr = $6;
        $$ = node;
    }
    ;

function_decl
    : FUNCTION IDENTIFIER '(' param_list ')' ':' IDENTIFIER block {
        auto fn = alloc_node(@$, FUNCTION_DECL);
        fn->fn_decl.name = intern($2);
        fn->fn_decl.first_param = $4.first;
        fn->fn_decl.param_count = $4.count;
        fn->fn_decl.return_type_hint = intern($7);
        fn->fn_decl.body = $8;
        $$ = fn;
    }
    | FUNCTION IDENTIFIER '(' param_list ')' block {
        auto fn = alloc_node(@$, FUNCTION_DECL);
        fn->fn_decl.name = intern($2);
        fn->fn_decl.first_param = $4.first;
        fn->fn_decl.param_count = $4.count;
        fn->fn_decl.return_type_hint = {0};
        fn->fn_decl.body = $6;
        $$ = fn;
    }
    ;

block
    : '{' statement_list '}' {
        auto blk = alloc_node(@$, BLOCK);
        blk->children.first_child = $2.first;
        blk->children.child_count = $2.count;
        $$ = blk;
    }
    | '{' '}' {
        auto blk = alloc_node(@$, BLOCK);
        blk->children.first_child = nullptr;
        blk->children.child_count = 0;
        $$ = blk;
    }
    ;

statement
    : var_decl ';'                 { $$ = $1; }
    | block                        { $$ = $1; }
    | function_decl                { $$ = $1; }
    | assign_expr ';'              { $$ = $1; }
    | if_stmt                      { $$ = $1; }
    | while_stmt                   { $$ = $1; }
    | return_stmt ';'              { $$ = $1; }
    | YIELD ';'                    { $$ = alloc_node(@$, YIELD_STMT); }
    | import_stmt ';'              { $$ = $1; }
    | WAIT '(' expr ')' ';' {
        auto w = alloc_node(@$, WAIT_STMT);
        w->wait_stmt.expr = $3;
        $$ = w;
    }
    | call_expr ';' {
        auto st = alloc_node(@$, EXPR_STMT);
        st->children.first_child = $1;
        st->children.child_count = 1;
        $$ = st;
    }
    ;

import_stmt
    : IMPORT STRING_LITERAL {
        auto import = alloc_node(@$, IMPORT_STMT);
        import->import_stmt.root = nullptr;
        import->import_stmt.path = intern($2);
        $$ = import;
    }

return_stmt
    : RETURN expr {
        auto ret = alloc_node(@$, RETURN_STMT);
        ret->unary.op = token::RETURN;
        ret->unary.operand = $2;
        $$ = ret;
    }
    | RETURN {
        auto ret = alloc_node(@$, RETURN_STMT);
        ret->unary.op = token::RETURN;
        ret->unary.operand = nullptr;
        $$ = ret;
    }
    ;

call_expr
    : IDENTIFIER '(' arg_list ')' {
        auto call = alloc_node(@$, CALL_EXPR);
        call->call.name = intern($1);
        call->call.first_arg = $3.first;
        call->call.arg_count = $3.count;
        call->call.is_async = false;
        $$ = call;
    }
    | IDENTIFIER '[' arg_list ']' {
        auto call = alloc_node(@$, CALL_EXPR);
        call->call.name = intern($1);
        call->call.first_arg = $3.first;
        call->call.arg_count = $3.count;
        call->call.is_async = true;
        $$ = call;
    }
    ;

assign_expr
    : IDENTIFIER '=' expr {
        auto target = alloc_node(@1, IDENTIFIER_EXPR);
        target->id_expr.name = intern($1);

        auto assign = alloc_node(@$, ASSIGN_EXPR);
        assign->assign.target = target;
        assign->assign.val = $3;
        $$ = assign;
    }
    ;

if_stmt
    : IF '(' expr ')' block {
        auto node = alloc_node(@$, IF_STMT);
        node->if_stmt.cond = $3;
        node->if_stmt.then_branch = $5;
        node->if_stmt.else_branch = nullptr;
        $$ = node;
    }
    | IF '(' expr ')' block ELSE block {
        auto node = alloc_node(@$, IF_STMT);
        node->if_stmt.cond = $3;
        node->if_stmt.then_branch = $5;
        node->if_stmt.else_branch = $7;
        $$ = node;
    }
    | IF '(' expr ')' block ELSE if_stmt {
        auto node = alloc_node(@$, IF_STMT);
        node->if_stmt.cond = $3;
        node->if_stmt.then_branch = $5;
        node->if_stmt.else_branch = $7;
        $$ = node;
    }
    ;

while_stmt
    : WHILE '(' expr ')' block {
        auto node = alloc_node(@$, WHILE_STMT);
        node->while_stmt.cond = $3;
        node->while_stmt.body = $5;
        $$ = node;
    }
    ;

expr
    : INT_LITERAL {
        auto node = alloc_node(@1, INT_LITERAL);
        node->int_val = $1;
        $$ = node;
    }
    | FLOAT_LITERAL {
        auto node = alloc_node(@1, FLOAT_LITERAL);
        node->float_val = $1;
        $$ = node;
    }
    | STRING_LITERAL {
        auto node = alloc_node(@1, STRING_LITERAL);
        node->str_val = intern($1);
        $$ = node;
    }
    | BOOL_LITERAL {
        auto node = alloc_node(@1, BOOL_LITERAL);
        node->bool_val = $1;
        $$ = node;
    }
    | IDENTIFIER {
        auto node = alloc_node(@1, IDENTIFIER_EXPR);
        node->id_expr.name = intern($1);
        $$ = node;
    }
    | call_expr { 
        $$ = $1; 
    }
    | '-' expr %prec UMINUS {
        auto node = alloc_node(@$, UNARY_EXPR);
        node->unary.op = '-';
        node->unary.operand = $2;
        $$ = node;
    }
    | expr '+' expr {
        auto node = alloc_node(@$, BINARY_EXPR);
        node->binop.op = '+';
        node->binop.left = $1;
        node->binop.right = $3;
        $$ = node;
    }
    | expr '-' expr {
        auto node = alloc_node(@$, BINARY_EXPR);
        node->binop.op = '-';
        node->binop.left = $1;
        node->binop.right = $3;
        $$ = node;
    }
    | expr '*' expr {
        auto node = alloc_node(@$, BINARY_EXPR);
        node->binop.op = '*';
        node->binop.left = $1;
        node->binop.right = $3;
        $$ = node;
    }
    | expr '/' expr {
        auto node = alloc_node(@$, BINARY_EXPR);
        node->binop.op = '/';
        node->binop.left = $1;
        node->binop.right = $3;
        $$ = node;
    }
    | expr EQ expr {
        auto node = alloc_node(@$, BINARY_EXPR);
        node->binop.op = token::EQ;
        node->binop.left = $1;
        node->binop.right = $3;
        $$ = node;
    }
    | expr '<' expr {
        auto node = alloc_node(@$, BINARY_EXPR);
        node->binop.op = '<';
        node->binop.left = $1;
        node->binop.right = $3;
        $$ = node;
    }
    | expr AS IDENTIFIER {
        auto node = alloc_node(@$, EXPLICIT_CAST_EXPR);
        node->cast_expr.operand = $1;
        node->cast_expr.target_type_hint = intern($3);
        $$ = node;
    }
    | '(' expr ')' {
        $$ = $2;
    }
    ;

%%

void adv::Parser::error(const location_type& loc, const std::string& message)
{
    log(E3001, loc);
}
