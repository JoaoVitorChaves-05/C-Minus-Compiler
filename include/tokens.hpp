#ifndef TOKENS_HPP
#define TOKENS_HPP

#include <cstdio>
#include <string>

enum TokenType {
    TOKEN_EOF = 0,
    // Palavras reservadas
    TOKEN_IF,
    TOKEN_ELSE = 258,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_RETURN,
    TOKEN_VOID,
    TOKEN_INT,
    // Identificadores e numeros
    TOKEN_ID,
    TOKEN_NUM,
    // Operadores e delimitadores
    TOKEN_PLUS,     // +
    TOKEN_MINUS,    // -
    TOKEN_TIMES,    // *
    TOKEN_OVER,     // /
    TOKEN_LT,       // <
    TOKEN_LE,       // <=
    TOKEN_GT,       // >
    TOKEN_GE,       // >=
    TOKEN_EQ,       // ==
    TOKEN_NEQ,      // !=
    TOKEN_ASSIGN,   // =
    TOKEN_SEMI,     // ;
    TOKEN_COMMA,    // ,
    TOKEN_LPAREN,   // (
    TOKEN_RPAREN,   // )
    TOKEN_LBRACK,   // [
    TOKEN_RBRACK,   // ]
    TOKEN_LBRACE,   // {
    TOKEN_RBRACE    // }
};

extern std::string current_lexeme;
extern int yylineno;
extern FILE* yyin;

int yylex();
const char* token_name(int token);

#endif // TOKENS_HPP