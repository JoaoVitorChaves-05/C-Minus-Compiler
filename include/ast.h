#ifndef AST_H
#define AST_H

#include <stddef.h>
#include <stdio.h>

#define AST_CHILD_CAPACITY 3

typedef enum {
    NODE_VAR_DECL,
    NODE_ARRAY_DECL,
    NODE_FUN_DECL,
    NODE_PARAM_VAR,
    NODE_PARAM_ARRAY,
    NODE_COMPOUND_STMT,
    NODE_IF_STMT,
    NODE_WHILE_STMT,
    NODE_RETURN_STMT,
    NODE_EXPR_STMT,
    NODE_ASSIGN_EXPR,
    NODE_OP_EXPR,
    NODE_VAR_EXPR,
    NODE_ARRAY_EXPR,
    NODE_CALL_EXPR,
    NODE_CONST_EXPR
} NodeKind;

typedef enum {
    DATA_VOID,
    DATA_INT
} DataType;

typedef struct ASTNode {
    NodeKind kind;
    DataType type;
    char *name;
    int value;
    int line;
    struct ASTNode *children[AST_CHILD_CAPACITY];
    size_t child_count;
    struct ASTNode *sibling;
} ASTNode;

ASTNode *new_node(NodeKind kind, int line);
char *ast_copy_string(const char *text);
int ast_set_name(ASTNode *node, const char *name);
int ast_add_child(ASTNode *node, ASTNode *child);
ASTNode *ast_child(ASTNode *node, size_t index);
ASTNode *append_sibling(ASTNode *list, ASTNode *node);
void free_ast(ASTNode *root);

void print_ast_text(ASTNode *root, int indent);
void print_tree(FILE *output, ASTNode *root);
void printTree(ASTNode *root);
int getHeight(ASTNode *root);
void fillBuffer(ASTNode *node, int level, int has_parent, int is_last);
int export_ast_dot(ASTNode *root, const char *filename);

#endif
