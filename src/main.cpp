#include <iostream>
#include <cstdio>
#include "ast.hpp"
#include "semantic.hpp"

extern FILE* yyin;
extern int yyparse();
extern ASTNode* ast_root;
extern bool syntax_has_error;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0] << " <arquivo.cm>\n";
        return 1;
    }

    FILE* source = std::fopen(argv[1], "r");
    if (!source) {
        std::cerr << "Erro ao abrir o arquivo: " << argv[1] << "\n";
        return 1;
    }

    yyin = source;

    yyparse();
    std::fclose(source);

    if (syntax_has_error) {
        std::cerr << "Compilacao encerrada devido a erros sintaticos.\n";
        free_ast(ast_root);
        return 1;
    }

    SymbolTable symbols;
    if (!analyze_semantics(ast_root, symbols)) {
        free_ast(ast_root);
        return 1;
    }

    std::cout << "=== TABELA DE SIMBOLOS ===\n";
    symbols.print(std::cout);

    std::cout << "=== ARVORE SINTATICA ABSTRATA (AST) ===\n";
    printTree(ast_root);

    export_ast_dot(ast_root, "ast.dot");
    std::cout << "\nArquivo 'ast.dot' gerado com sucesso.\n";

    free_ast(ast_root);
    return 0;
}