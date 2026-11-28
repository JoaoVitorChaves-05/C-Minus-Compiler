%{
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include "ast.hpp"

extern int yylex();
extern int yylineno;

ASTNode* ast_root = nullptr;
bool syntax_has_error = false;

void yyerror(const char* msg);
%}

%error-verbose
%locations
/* Keep the shift into local-declaration recovery over the empty statement list. */
%expect 1

%union {
    int int_val;
    char* str_val;
    DataType data_type;
    ASTNode* node;
}

%token IF INT RETURN VOID WHILE
%token <str_val> ID
%token <int_val> NUM
%token PLUS MINUS TIMES OVER
%token LT LE GT GE EQ NEQ
%token ASSIGN SEMI COMMA
%token LPAREN RPAREN LBRACK RBRACK LBRACE RBRACE

%nonassoc IF_WITHOUT_ELSE
%nonassoc ELSE

%type <node> program declaration_list declaration var_declaration fun_declaration
%type <node> params param_list param compound_stmt local_declarations statement_list
%type <node> statement expression_stmt selection_stmt iteration_stmt return_stmt
%type <node> expression var simple_expression additive_expression term factor call
%type <node> args arg_list
%type <data_type> type_specifier
%type <str_val> relop addop mulop

%%

program
    : declaration_list
      { ast_root = $1; }
    ;

declaration_list
    : declaration_list declaration
      { $$ = append_sibling($1, $2); }
    | declaration
      { $$ = $1; }
    ;

declaration
    : var_declaration
      { $$ = $1; }
    | fun_declaration
      { $$ = $1; }
    | error SEMI
      { yyerrok; $$ = nullptr; }
    ;

var_declaration
    : type_specifier ID SEMI
      {
          $$ = new_node(NodeKind::VAR_DECL, @1.first_line);
          $$->type = $1;
          $$->name = $2;
          std::free($2);
      }
    | type_specifier ID LBRACK NUM RBRACK SEMI
      {
          $$ = new_node(NodeKind::ARRAY_DECL, @1.first_line);
          $$->type = $1;
          $$->name = $2;
          $$->value = $4;
          std::free($2);
      }
    ;

type_specifier
    : INT  { $$ = DataType::INT_TYPE; }
    | VOID { $$ = DataType::VOID_TYPE; }
    ;

fun_declaration
    : type_specifier ID LPAREN params RPAREN compound_stmt
      {
          $$ = new_node(NodeKind::FUN_DECL, @1.first_line);
          $$->type = $1;
          $$->name = $2;
          $$->children.push_back($4); // parâmetros
          $$->children.push_back($6); // corpo
          std::free($2);
      }
    ;

params
    : param_list
      { $$ = $1; }
    | VOID
      { $$ = nullptr; }
    ;

param_list
    : param_list COMMA param
      { $$ = append_sibling($1, $3); }
    | param
      { $$ = $1; }
    ;

param
    : type_specifier ID
      {
          $$ = new_node(NodeKind::PARAM_VAR, @1.first_line);
          $$->type = $1;
          $$->name = $2;
          std::free($2);
      }
    | type_specifier ID LBRACK RBRACK
      {
          $$ = new_node(NodeKind::PARAM_ARRAY, @1.first_line);
          $$->type = $1;
          $$->name = $2;
          std::free($2);
      }
    ;

compound_stmt
    : LBRACE local_declarations statement_list RBRACE
      {
          $$ = new_node(NodeKind::COMPOUND_STMT, @1.first_line);
          $$->children.push_back($2); // declarações locais
          $$->children.push_back($3); // lista de comandos
      }
    ;

local_declarations
    : local_declarations var_declaration
      { $$ = append_sibling($1, $2); }
    | local_declarations error SEMI
      { yyerrok; $$ = $1; }
    | /* vazio */
      { $$ = nullptr; }
    ;

statement_list
    : statement_list statement
      { $$ = append_sibling($1, $2); }
    | /* vazio */
      { $$ = nullptr; }
    ;

statement
    : expression_stmt { $$ = $1; }
    | compound_stmt   { $$ = $1; }
    | selection_stmt  { $$ = $1; }
    | iteration_stmt  { $$ = $1; }
    | return_stmt     { $$ = $1; }
    ;

expression_stmt
    : expression SEMI
      {
          $$ = new_node(NodeKind::EXPR_STMT, @1.first_line);
          $$->children.push_back($1);
      }
    | SEMI
      { $$ = nullptr; }
    | error SEMI
      { yyerrok; $$ = nullptr; }
    ;

selection_stmt
    : IF LPAREN expression RPAREN statement %prec IF_WITHOUT_ELSE
      {
          $$ = new_node(NodeKind::IF_STMT, @1.first_line);
          $$->children.push_back($3); // condição
          $$->children.push_back($5); // ramo then
      }
    | IF LPAREN expression RPAREN statement ELSE statement
      {
          $$ = new_node(NodeKind::IF_STMT, @1.first_line);
          $$->children.push_back($3); // condição
          $$->children.push_back($5); // ramo then
          $$->children.push_back($7); // ramo else
      }
    | IF LPAREN error RPAREN statement
      { yyerrok; $$ = nullptr; }
    ;

iteration_stmt
    : WHILE LPAREN expression RPAREN statement
      {
          $$ = new_node(NodeKind::WHILE_STMT, @1.first_line);
          $$->children.push_back($3); // condição
          $$->children.push_back($5); // corpo do loop
      }
    | WHILE LPAREN error RPAREN statement
      { yyerrok; $$ = nullptr; }
    ;

return_stmt
    : RETURN SEMI
      {
          $$ = new_node(NodeKind::RETURN_STMT, @1.first_line);
      }
    | RETURN expression SEMI
      {
          $$ = new_node(NodeKind::RETURN_STMT, @1.first_line);
          $$->children.push_back($2);
      }
    ;

expression
    : var ASSIGN expression
      {
          $$ = new_node(NodeKind::ASSIGN_EXPR, @1.first_line);
          $$->children.push_back($1);
          $$->children.push_back($3);
      }
    | simple_expression
      { $$ = $1; }
    ;

var
    : ID
      {
          $$ = new_node(NodeKind::VAR_EXPR, @1.first_line);
          $$->name = $1;
          std::free($1);
      }
    | ID LBRACK expression RBRACK
      {
          $$ = new_node(NodeKind::ARRAY_EXPR, @1.first_line);
          $$->name = $1;
          $$->children.push_back($3);
          std::free($1);
      }
    ;

simple_expression
    : additive_expression relop additive_expression
      {
          $$ = new_node(NodeKind::OP_EXPR, @1.first_line);
          $$->name = $2;
          $$->children.push_back($1);
          $$->children.push_back($3);
          std::free($2);
      }
    | additive_expression
      { $$ = $1; }
    ;

relop
    : LE  { $$ = strdup("<="); }
    | LT  { $$ = strdup("<"); }
    | GT  { $$ = strdup(">"); }
    | GE  { $$ = strdup(">="); }
    | EQ  { $$ = strdup("=="); }
    | NEQ { $$ = strdup("!="); }
    ;

additive_expression
    : additive_expression addop term
      {
          $$ = new_node(NodeKind::OP_EXPR, @1.first_line);
          $$->name = $2;
          $$->children.push_back($1);
          $$->children.push_back($3);
          std::free($2);
      }
    | term
      { $$ = $1; }
    ;

addop
    : PLUS  { $$ = strdup("+"); }
    | MINUS { $$ = strdup("-"); }
    ;

term
    : term mulop factor
      {
          $$ = new_node(NodeKind::OP_EXPR, @1.first_line);
          $$->name = $2;
          $$->children.push_back($1);
          $$->children.push_back($3);
          std::free($2);
      }
    | factor
      { $$ = $1; }
    ;

mulop
    : TIMES { $$ = strdup("*"); }
    | OVER  { $$ = strdup("/"); }
    ;

factor
    : LPAREN expression RPAREN
      { $$ = $2; }
    | var
      { $$ = $1; }
    | call
      { $$ = $1; }
    | NUM
      {
          $$ = new_node(NodeKind::CONST_EXPR, @1.first_line);
          $$->value = $1;
      }
    ;

call
    : ID LPAREN args RPAREN
      {
          $$ = new_node(NodeKind::CALL_EXPR, @1.first_line);
          $$->name = $1;
          $$->children.push_back($3);
          std::free($1);
      }
    ;

args
    : arg_list
      { $$ = $1; }
    | /* vazio */
      { $$ = nullptr; }
    ;

arg_list
    : arg_list COMMA expression
      { $$ = append_sibling($1, $3); }
    | expression
      { $$ = $1; }
    ;

%%

void yyerror(const char* msg) {
    syntax_has_error = true;
    std::string err(msg);
    std::string unexpected = "desconhecido";
    std::string expected = "desconhecido";

    size_t unexp_pos = err.find("unexpected ");
    size_t exp_pos = err.find(", expecting ");
    if (unexp_pos != std::string::npos && exp_pos != std::string::npos) {
        unexpected = err.substr(unexp_pos + 11, exp_pos - (unexp_pos + 11));
        expected = err.substr(exp_pos + 12);
    } else if (unexp_pos != std::string::npos) {
        unexpected = err.substr(unexp_pos + 11);
    }

    size_t alternative_pos = expected.find(" or ");
    if (alternative_pos != std::string::npos) {
        expected = expected.substr(0, alternative_pos);
    }

    if (unexpected == "$end") unexpected = "EOF";
    if (expected == "$end") expected = "EOF";
    if (expected == "desconhecido" && unexpected == "EOF") expected = "RBRACE";

    const int error_line = yylloc.first_line > 0 ? yylloc.first_line : yylineno;
    std::cerr << "ERRO SINTATICO: token inesperado ''" << unexpected
              << "'', esperado ''" << expected << "'' LINHA: " << error_line << "\n";
}