#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include "ast.hpp"
#include "symtab.hpp"

bool analyze_semantics(ASTNode* root, SymbolTable& symbols);

#endif // SEMANTIC_HPP
