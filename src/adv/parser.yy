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
%type <adv_node_t*>  param
%type <NodeList>     param_list
%type <NodeList>     arg_list
%type <adv_node_t*>  block
%type <adv_node_t*>  init_expr
%type <vm_string_h>  type_hint

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
        prog->stmts.first_stmt = $1.first;
        prog->stmts.stmt_count = $1.count;
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

param
    : IDENTIFIER {
        log(E3006, @1, $1);
        auto p = alloc_node(@1, PARAM_DECL);
        p->param_decl.name = intern($1);
        p->param_decl.type_hint = intern("<error>"s);
        $$ = p;
    }
    | IDENTIFIER ':' IDENTIFIER {
        auto p = alloc_node(@1, PARAM_DECL);
        p->param_decl.name = intern($1);
        p->param_decl.type_hint = intern($3);
        $$ = p;
    }
    ;

param_list:
    /* empty */            { $$ = list_init(nullptr); }
    | param                { $$ = list_init($1); }
    | param_list ',' param { $$ = list_add($1, $3); }
    ;

type_hint:
    /* empty */ { $$ = vm_string_h{0}; }
    | ':' IDENTIFIER { $$ = intern($2); }
    ;

init_expr:
    /* empty */ { $$ = nullptr; }
    | '=' expr { $$ = $2; }
    ;

var_decl
    : VAR IDENTIFIER type_hint init_expr {
        if ($3.handle == 0 && $4 == nullptr) {
            log(E3007, @2, $2);
        }
        auto node = alloc_node(@$, VAR_DECL);
        node->var_decl.name = intern($2);
        node->var_decl.type_hint = $3;
        node->var_decl.init_expr = $4;
        $$ = node;
    }
    ;

function_decl
    : FUNCTION IDENTIFIER '(' param_list ')' ':' IDENTIFIER block {
        auto fn = alloc_node(@$, FUNCTION_DECL);
        fn->func_decl.name = intern($2);
        fn->func_decl.first_param = $4.first;
        fn->func_decl.param_count = $4.count;
        fn->func_decl.return_type_hint = intern($7);
        fn->func_decl.body = $8;
        $$ = fn;
    }
    | FUNCTION IDENTIFIER '(' param_list ')' block {
        auto fn = alloc_node(@$, FUNCTION_DECL);
        fn->func_decl.name = intern($2);
        fn->func_decl.first_param = $4.first;
        fn->func_decl.param_count = $4.count;
        fn->func_decl.return_type_hint = {0};
        fn->func_decl.body = $6;
        $$ = fn;
    }
    ;

block
    : '{' statement_list '}' {
        auto blk = alloc_node(@$, BLOCK);
        blk->stmts.first_stmt = $2.first;
        blk->stmts.stmt_count = $2.count;
        $$ = blk;
    }
    | '{' '}' {
        auto blk = alloc_node(@$, BLOCK);
        blk->stmts.first_stmt = nullptr;
        blk->stmts.stmt_count = 0;
        $$ = blk;
    }
    ;

statement
    : var_decl ';'                 { $$ = $1; }
    | block                        { $$ = $1; }
    | function_decl                { $$ = $1; }
    | if_stmt                      { $$ = $1; }
    | while_stmt                   { $$ = $1; }
    | return_stmt ';'              { $$ = $1; }
    | YIELD ';'                    { $$ = alloc_node(@$, YIELD_STMT); }
    | import_stmt ';'              { $$ = $1; }
    | WAIT '(' expr ')' ';' {
        auto w = alloc_node(@$, WAIT_STMT);
        w->expr_stmt.expr = $3;
        $$ = w;
    }
    | expr ';' {
        auto st = alloc_node(@$, EXPR_STMT);
        st->expr_stmt.expr = $1;
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
        ret->expr_stmt.expr = $2;
        $$ = ret;
    }
    | RETURN {
        auto ret = alloc_node(@$, RETURN_STMT);
        ret->expr_stmt.expr = nullptr;
        $$ = ret;
    }
    ;

call_expr
    : IDENTIFIER '(' arg_list ')' {
        auto call = alloc_node(@$, CALL_EXPR);
        call->call_expr.name = intern($1);
        call->call_expr.first_arg = $3.first;
        call->call_expr.arg_count = $3.count;
        call->call_expr.is_async = false;
        $$ = call;
    }
    | IDENTIFIER '[' arg_list ']' {
        auto call = alloc_node(@$, CALL_EXPR);
        call->call_expr.name = intern($1);
        call->call_expr.first_arg = $3.first;
        call->call_expr.arg_count = $3.count;
        call->call_expr.is_async = true;
        $$ = call;
    }
    ;

assign_expr
    : IDENTIFIER '=' expr {
        auto target = alloc_node(@1, IDENTIFIER_EXPR);
        target->id_expr.name = intern($1);

        auto assign = alloc_node(@$, ASSIGN_EXPR);
        assign->assign_expr.target = target;
        assign->assign_expr.val = $3;
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
    | assign_expr { 
        $$ = $1; 
    }
    | call_expr { 
        $$ = $1; 
    }
    | '-' expr %prec UMINUS {
        auto node = alloc_node(@$, UNARY_EXPR);
        node->unary_expr.op = '-';
        node->unary_expr.operand = $2;
        $$ = node;
    }
    | expr '+' expr {
        auto node = alloc_node(@$, BINARY_EXPR);
        node->binary_expr.op = '+';
        node->binary_expr.left = $1;
        node->binary_expr.right = $3;
        $$ = node;
    }
    | expr '-' expr {
        auto node = alloc_node(@$, BINARY_EXPR);
        node->binary_expr.op = '-';
        node->binary_expr.left = $1;
        node->binary_expr.right = $3;
        $$ = node;
    }
    | expr '*' expr {
        auto node = alloc_node(@$, BINARY_EXPR);
        node->binary_expr.op = '*';
        node->binary_expr.left = $1;
        node->binary_expr.right = $3;
        $$ = node;
    }
    | expr '/' expr {
        auto node = alloc_node(@$, BINARY_EXPR);
        node->binary_expr.op = '/';
        node->binary_expr.left = $1;
        node->binary_expr.right = $3;
        $$ = node;
    }
    | expr EQ expr {
        auto node = alloc_node(@$, BINARY_EXPR);
        node->binary_expr.op = token::EQ;
        node->binary_expr.left = $1;
        node->binary_expr.right = $3;
        $$ = node;
    }
    | expr '<' expr {
        auto node = alloc_node(@$, BINARY_EXPR);
        node->binary_expr.op = '<';
        node->binary_expr.left = $1;
        node->binary_expr.right = $3;
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
