%{
#include "scanner.hpp"
#define YY_NO_UNISTD_H
#define YY_DECL int adv::Scanner::yylex(adv::Parser::semantic_type* yylval, adv::Parser::location_type* yylloc)
#define YY_USER_ACTION action(*yylloc);
#define ast_location { \
    context().intern({yylloc->begin.filename->c_str(), yylloc->begin.filename->length()}), \
    adv_uint32_t(yylloc->begin.line), \
    adv_uint32_t(yylloc->begin.column) }
#define log(status, ...) log<ADV_STATUS_##status>(ast_location __VA_OPT__(,) __VA_ARGS__)
#define yy_text std::string { YYText(), size_t(YYLeng()) }
#define yy_text_view std::string_view { YYText(), size_t(YYLeng()) }
#define parse_float(str) parseFloat(ast_location, str)
#define parse_int(str) parseInt(ast_location, str)
using namespace std::literals;
typedef adv::Parser::token token;
%}

%option c++
%option noyywrap
%option yylineno
%option outfile="scanner.cpp"

%x COMMENT

FLOAT [0-9]*\.[0-9]+([eE][-+]?[0-9]+)?
BOOL  (true|false)
INT   [0-9]+
STR   \"(\\.|[^\\"\r\n])*\"
STR_U \"(\\.|[^\\"\r\n])*
ID    [a-zA-Z_][a-zA-Z0-9_]*

%%

%{
    yylloc->step();
%}

"import"         { return token::IMPORT; }
"function"       { return token::FUNCTION; }
"var"            { return token::VAR; }
"as"             { return token::AS; }
"or"             { return token::OR; }
"and"            { return token::AND; }
"if"             { return token::IF; }
"else"           { return token::ELSE; }
"while"          { return token::WHILE; }
"yield"          { return token::YIELD; }
"wait"           { return token::WAIT; }
"return"         { return token::RETURN; }

{BOOL}           { yylval->emplace<bool>(yy_text_view[0] == 't'); return token::BOOL_LITERAL; }
{FLOAT}          { yylval->emplace<float>(parse_float(yy_text_view)); return token::FLOAT_LITERAL; }
{INT}            { yylval->emplace<int32_t>(parse_int(yy_text_view)); return token::INT_LITERAL; }
{ID}             { yylval->emplace<std::string>(yy_text); return token::IDENTIFIER; }
{STR}            { auto str = yy_text_view.substr(1, yy_text_view.length() - 2); yylval->emplace<std::string>(unescape(ast_location, str)); return token::STRING_LITERAL; }
{STR_U}          { log(E3003, yy_text_view); yylval->emplace<std::string>(""s); return token::STRING_LITERAL; }

"=="             { return token::EQ; }
"!="             { return token::NE; }
"<="             { return token::LE; }
">="             { return token::GE; }

"<<<"            { return token::LSHL; }
">>>"            { return token::LSHR; }
"<<"             { return token::ASHL; }
">>"             { return token::ASHR; }
"="              { return '='; }
"+"              { return '+'; }
"-"              { return '-'; }
"*"              { return '*'; }
"/"              { return '/'; }
"%"              { return '%'; }
"<"              { return '<'; }
">"              { return '>'; }
"~"              { return '~'; }
"|"              { return '|'; }
"&"              { return '&'; }
"^"              { return '^'; }
"!"              { return '!'; }
"("              { return '('; }
")"              { return ')'; }
"["              { return '['; }
"]"              { return ']'; }
"{"              { return '{'; }
"}"              { return '}'; }
","              { return ','; }
":"              { return ':'; }
";"              { return ';'; }

"/*"             { BEGIN(COMMENT); }
<COMMENT>"*/"    { BEGIN(INITIAL); }
<COMMENT>\r?\n   { yylloc->lines(); }
<COMMENT>.       { }
<COMMENT><<EOF>> { log(E3004); BEGIN(INITIAL); }

\r?\n            { yylloc->lines(); }
[ \t]+           { }
"//".*           { }
.                { log(E3001, yy_text_view); }

%%

int yyFlexLexer::yylex() {
    throw std::runtime_error("Bad call to yyFlexLexer::yylex()");
}
