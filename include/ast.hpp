#ifndef AST_HPP
#define AST_HPP

#include <string>
#include <vector>
#include <fstream>

enum class NodeKind {
    // Declarações
    VAR_DECL,
    ARRAY_DECL,
    FUN_DECL,
    PARAM_VAR,
    PARAM_ARRAY,
    // Comandos (Statements)
    COMPOUND_STMT,
    IF_STMT,
    WHILE_STMT,
    RETURN_STMT,
    EXPR_STMT,
    // Expressões
    ASSIGN_EXPR,
    OP_EXPR,
    VAR_EXPR,
    ARRAY_EXPR,
    CALL_EXPR,
    CONST_EXPR
};

enum class DataType {
    VOID_TYPE,
    INT_TYPE
};

struct ASTNode {
    NodeKind kind;
    DataType type = DataType::VOID_TYPE;
    std::string name;      // Nome de identificador ou operador (+, -, ==, etc.)
    int value = 0;         // Valor numérico (constantes ou tamanho de vetor)
    int line = 0;          // Linha no código-fonte

    std::vector<ASTNode*> children; // Filhos estruturais (parâmetros, corpo, if/else, operandos)
    ASTNode* sibling = nullptr;     // Próximo nó na mesma lista (declarações, comandos, args)

    ASTNode(NodeKind k, int ln) : kind(k), line(ln) {}
};

// Funções auxiliares de construção
ASTNode* new_node(NodeKind kind, int line);
ASTNode* append_sibling(ASTNode* list, ASTNode* node);
void free_ast(ASTNode* root);

// Saídas obrigatórias da AST (textual e Graphviz .dot)
void print_ast_text(ASTNode* root, int indent = 0);
void printTree(ASTNode* root);
int getHeight(ASTNode* root);
void fillBuffer(ASTNode* node, int level, int left, int right);
void export_ast_dot(ASTNode* root, const std::string& filename);

#endif