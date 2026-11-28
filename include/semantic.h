#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "symtab.h"

int analyze_semantics(ASTNode *root, SymbolTable *symbols);

#endif
