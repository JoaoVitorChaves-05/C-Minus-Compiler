#include <iostream>
#include <cstdio>
#include "tokens.hpp"

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

    int tok;
    while ((tok = yylex()) != TOKEN_EOF) {
        std::cout << "Linha " << yylineno
                  << " | Token: " << token_name(tok)
                  << " | Lexema: " << current_lexeme << "\n";
    }

    std::fclose(source);
    return 0;
}