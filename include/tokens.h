#ifndef TOKENS_H
#define TOKENS_H

#include <stdio.h>

extern int yylineno;
extern FILE *yyin;

int yylex(void);
const char *token_name(int token);

#endif
