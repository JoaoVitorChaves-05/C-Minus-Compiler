#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "gci.h"
#include "semantic.h"
#include "symtab.h"
#include "tokens.h"

extern int yyparse(void);
extern ASTNode *ast_root;
extern int syntax_has_error;

int main(int argc, char **argv) {
    FILE *source;
    SymbolTable symbols;
    GCI gci = GCI_INITIALIZER;
    int parse_result;

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo.cm>\n", argv[0]);
        return EXIT_FAILURE;
    }

    source = fopen(argv[1], "r");
    if (!source) {
        fprintf(stderr, "Erro ao abrir o arquivo: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    yyin = source;
    parse_result = yyparse();
    fclose(source);

    if (parse_result != 0 || syntax_has_error) {
        fputs("Compilacao encerrada devido a erros sintaticos.\n", stderr);
        free_ast(ast_root);
        return EXIT_FAILURE;
    }

    if (!symbol_table_init(&symbols)) {
        fputs("Erro: nao foi possivel inicializar a tabela de simbolos.\n", stderr);
        free_ast(ast_root);
        return EXIT_FAILURE;
    }

    if (!analyze_semantics(ast_root, &symbols)) {
        symbol_table_destroy(&symbols);
        free_ast(ast_root);
        return EXIT_FAILURE;
    }

    fputs("=== TABELA DE SIMBOLOS ===\n", stdout);
    if (!symbol_table_print(&symbols, stdout)) {
        fputs("Erro ao imprimir a tabela de simbolos.\n", stderr);
        symbol_table_destroy(&symbols);
        free_ast(ast_root);
        return EXIT_FAILURE;
    }

    fputs("=== ARVORE SINTATICA ABSTRATA (AST) ===\n", stdout);
    print_tree(stdout, ast_root);

    fputs("\n=== CODIGO INTERMEDIARIO (QUADRUPLAS) ===\n", stdout);
    if (!generate_quads(ast_root, &gci)) {
        fputs("Erro: nao foi possivel gerar o codigo intermediario.\n", stderr);
        clear_quads(&gci);
        symbol_table_destroy(&symbols);
        free_ast(ast_root);
        return EXIT_FAILURE;
    }
    print_quads(stdout, &gci);
    clear_quads(&gci);

    if (!export_ast_dot(ast_root, "ast.dot")) {
        fputs("Erro ao gravar o arquivo 'ast.dot'.\n", stderr);
        symbol_table_destroy(&symbols);
        free_ast(ast_root);
        return EXIT_FAILURE;
    }
    fputs("\nArquivo 'ast.dot' gerado com sucesso.\n", stdout);

    symbol_table_destroy(&symbols);
    free_ast(ast_root);
    return EXIT_SUCCESS;
}
