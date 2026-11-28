%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

extern int yylex(void);
extern int yylineno;

ASTNode *ast_root = NULL;
int syntax_has_error = 0;

void yyerror(const char *message);
%}

%error-verbose
%locations
/* Keep the shift into local-declaration recovery over the empty statement list. */
%expect 1

%union {
    int int_val;
    char *str_val;
    DataType data_type;
    ASTNode *node;
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
%destructor { free($$); } <str_val>

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
      { yyerrok; $$ = NULL; }
    ;

var_declaration
    : type_specifier ID SEMI
      {
          $$ = new_node(NODE_VAR_DECL, @1.first_line);
          $$->type = $1;
          ast_set_name($$, $2);
          free($2);
      }
    | type_specifier ID LBRACK NUM RBRACK SEMI
      {
          $$ = new_node(NODE_ARRAY_DECL, @1.first_line);
          $$->type = $1;
          $$->value = $4;
          ast_set_name($$, $2);
          free($2);
      }
    ;

type_specifier
    : INT  { $$ = DATA_INT; }
    | VOID { $$ = DATA_VOID; }
    ;

fun_declaration
    : type_specifier ID LPAREN params RPAREN compound_stmt
      {
          $$ = new_node(NODE_FUN_DECL, @1.first_line);
          $$->type = $1;
          ast_set_name($$, $2);
          ast_add_child($$, $4);
          ast_add_child($$, $6);
          free($2);
      }
    ;

params
    : param_list
      { $$ = $1; }
    | VOID
      { $$ = NULL; }
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
          $$ = new_node(NODE_PARAM_VAR, @1.first_line);
          $$->type = $1;
          ast_set_name($$, $2);
          free($2);
      }
    | type_specifier ID LBRACK RBRACK
      {
          $$ = new_node(NODE_PARAM_ARRAY, @1.first_line);
          $$->type = $1;
          ast_set_name($$, $2);
          free($2);
      }
    ;

compound_stmt
    : LBRACE local_declarations statement_list RBRACE
      {
          $$ = new_node(NODE_COMPOUND_STMT, @1.first_line);
          ast_add_child($$, $2);
          ast_add_child($$, $3);
      }
    ;

local_declarations
    : local_declarations var_declaration
      { $$ = append_sibling($1, $2); }
    | local_declarations error SEMI
      { yyerrok; $$ = $1; }
    | /* empty */
      { $$ = NULL; }
    ;

statement_list
    : statement_list statement
      { $$ = append_sibling($1, $2); }
    | /* empty */
      { $$ = NULL; }
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
          $$ = new_node(NODE_EXPR_STMT, @1.first_line);
          ast_add_child($$, $1);
      }
    | SEMI
      { $$ = NULL; }
    | error SEMI
      { yyerrok; $$ = NULL; }
    ;

selection_stmt
    : IF LPAREN expression RPAREN statement %prec IF_WITHOUT_ELSE
      {
          $$ = new_node(NODE_IF_STMT, @1.first_line);
          ast_add_child($$, $3);
          ast_add_child($$, $5);
      }
    | IF LPAREN expression RPAREN statement ELSE statement
      {
          $$ = new_node(NODE_IF_STMT, @1.first_line);
          ast_add_child($$, $3);
          ast_add_child($$, $5);
          ast_add_child($$, $7);
      }
    | IF LPAREN error RPAREN statement
      { yyerrok; $$ = NULL; }
    ;

iteration_stmt
    : WHILE LPAREN expression RPAREN statement
      {
          $$ = new_node(NODE_WHILE_STMT, @1.first_line);
          ast_add_child($$, $3);
          ast_add_child($$, $5);
      }
    | WHILE LPAREN error RPAREN statement
      { yyerrok; $$ = NULL; }
    ;

return_stmt
    : RETURN SEMI
      { $$ = new_node(NODE_RETURN_STMT, @1.first_line); }
    | RETURN expression SEMI
      {
          $$ = new_node(NODE_RETURN_STMT, @1.first_line);
          ast_add_child($$, $2);
      }
    ;

expression
    : var ASSIGN expression
      {
          $$ = new_node(NODE_ASSIGN_EXPR, @1.first_line);
          ast_add_child($$, $1);
          ast_add_child($$, $3);
      }
    | simple_expression
      { $$ = $1; }
    ;

var
    : ID
      {
          $$ = new_node(NODE_VAR_EXPR, @1.first_line);
          ast_set_name($$, $1);
          free($1);
      }
    | ID LBRACK expression RBRACK
      {
          $$ = new_node(NODE_ARRAY_EXPR, @1.first_line);
          ast_set_name($$, $1);
          ast_add_child($$, $3);
          free($1);
      }
    ;

simple_expression
    : additive_expression relop additive_expression
      {
          $$ = new_node(NODE_OP_EXPR, @1.first_line);
          ast_set_name($$, $2);
          ast_add_child($$, $1);
          ast_add_child($$, $3);
          free($2);
      }
    | additive_expression
      { $$ = $1; }
    ;

relop
    : LE  { $$ = ast_copy_string("<="); }
    | LT  { $$ = ast_copy_string("<"); }
    | GT  { $$ = ast_copy_string(">"); }
    | GE  { $$ = ast_copy_string(">="); }
    | EQ  { $$ = ast_copy_string("=="); }
    | NEQ { $$ = ast_copy_string("!="); }
    ;

additive_expression
    : additive_expression addop term
      {
          $$ = new_node(NODE_OP_EXPR, @1.first_line);
          ast_set_name($$, $2);
          ast_add_child($$, $1);
          ast_add_child($$, $3);
          free($2);
      }
    | term
      { $$ = $1; }
    ;

addop
    : PLUS  { $$ = ast_copy_string("+"); }
    | MINUS { $$ = ast_copy_string("-"); }
    ;

term
    : term mulop factor
      {
          $$ = new_node(NODE_OP_EXPR, @1.first_line);
          ast_set_name($$, $2);
          ast_add_child($$, $1);
          ast_add_child($$, $3);
          free($2);
      }
    | factor
      { $$ = $1; }
    ;

mulop
    : TIMES { $$ = ast_copy_string("*"); }
    | OVER  { $$ = ast_copy_string("/"); }
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
          $$ = new_node(NODE_CONST_EXPR, @1.first_line);
          $$->value = $1;
      }
    ;

call
    : ID LPAREN args RPAREN
      {
          $$ = new_node(NODE_CALL_EXPR, @1.first_line);
          ast_set_name($$, $1);
          ast_add_child($$, $3);
          free($1);
      }
    ;

args
    : arg_list
      { $$ = $1; }
    | /* empty */
      { $$ = NULL; }
    ;

arg_list
    : arg_list COMMA expression
      { $$ = append_sibling($1, $3); }
    | expression
      { $$ = $1; }
    ;

%%

static void copy_message_part(char *destination, size_t capacity,
                              const char *start, size_t length) {
    if (capacity == 0) return;
    if (length >= capacity) length = capacity - 1;
    memcpy(destination, start, length);
    destination[length] = '\0';
}

void yyerror(const char *message) {
    char unexpected[128] = "desconhecido";
    char expected[128] = "desconhecido";
    const char *unexpected_start = strstr(message, "unexpected ");
    const char *expected_start = strstr(message, ", expecting ");
    int error_line = yylloc.first_line > 0 ? yylloc.first_line : yylineno;

    syntax_has_error = 1;
    if (unexpected_start) {
        unexpected_start += strlen("unexpected ");
        if (expected_start && expected_start > unexpected_start) {
            copy_message_part(
                unexpected, sizeof(unexpected), unexpected_start,
                (size_t)(expected_start - unexpected_start)
            );
        } else {
            snprintf(unexpected, sizeof(unexpected), "%s", unexpected_start);
        }
    }

    if (expected_start) {
        const char *alternative;
        expected_start += strlen(", expecting ");
        snprintf(expected, sizeof(expected), "%s", expected_start);
        alternative = strstr(expected, " or ");
        if (alternative) expected[alternative - expected] = '\0';
    }

    if (strcmp(unexpected, "$end") == 0) {
        snprintf(unexpected, sizeof(unexpected), "EOF");
    }
    if (strcmp(expected, "$end") == 0) {
        snprintf(expected, sizeof(expected), "EOF");
    }
    if (strcmp(expected, "desconhecido") == 0 &&
        strcmp(unexpected, "EOF") == 0) {
        snprintf(expected, sizeof(expected), "RBRACE");
    }

    fprintf(stderr,
            "ERRO SINTATICO: token inesperado ''%s'', esperado ''%s'' LINHA: %d\n",
            unexpected, expected, error_line);
}
