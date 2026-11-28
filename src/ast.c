#include "ast.h"

#include <stdlib.h>
#include <string.h>

static void allocation_failure(void) {
    fputs("Erro: memoria insuficiente ao construir a AST.\n", stderr);
    exit(EXIT_FAILURE);
}

static const char *type_name(DataType type) {
    return type == DATA_INT ? "int" : "void";
}

char *ast_copy_string(const char *text) {
    size_t length;
    char *copy;
    if (!text) return NULL;
    length = strlen(text) + 1;
    copy = (char *)malloc(length);
    if (!copy) allocation_failure();
    memcpy(copy, text, length);
    return copy;
}

ASTNode *new_node(NodeKind kind, int line) {
    ASTNode *node = (ASTNode *)calloc(1, sizeof(ASTNode));
    if (!node) allocation_failure();
    node->kind = kind;
    node->type = DATA_VOID;
    node->line = line;
    return node;
}

int ast_set_name(ASTNode *node, const char *name) {
    char *copy;
    if (!node || !name) return 0;
    copy = ast_copy_string(name);
    free(node->name);
    node->name = copy;
    return 1;
}

int ast_add_child(ASTNode *node, ASTNode *child) {
    if (!node || node->child_count >= AST_CHILD_CAPACITY) {
        fputs("Erro interno: capacidade de filhos da AST excedida.\n", stderr);
        exit(EXIT_FAILURE);
    }
    node->children[node->child_count++] = child;
    return 1;
}

ASTNode *ast_child(ASTNode *node, size_t index) {
    if (!node || index >= node->child_count) return NULL;
    return node->children[index];
}

ASTNode *append_sibling(ASTNode *list, ASTNode *node) {
    ASTNode *current;
    if (!list) return node;
    if (!node) return list;
    current = list;
    while (current->sibling) current = current->sibling;
    current->sibling = node;
    return list;
}

void free_ast(ASTNode *root) {
    size_t i;
    if (!root) return;
    for (i = 0; i < root->child_count; ++i) {
        free_ast(root->children[i]);
    }
    free_ast(root->sibling);
    free(root->name);
    free(root);
}

static void print_node_label(FILE *output, const ASTNode *node) {
    switch (node->kind) {
        case NODE_VAR_DECL:
            fprintf(output, "VarDecl: %s %s (linha %d)",
                    type_name(node->type), node->name, node->line);
            break;
        case NODE_ARRAY_DECL:
            fprintf(output, "ArrayDecl: %s %s[%d] (linha %d)",
                    type_name(node->type), node->name, node->value, node->line);
            break;
        case NODE_FUN_DECL:
            fprintf(output, "FunDecl: %s %s (linha %d)",
                    type_name(node->type), node->name, node->line);
            break;
        case NODE_PARAM_VAR:
            fprintf(output, "Param: %s %s", type_name(node->type), node->name);
            break;
        case NODE_PARAM_ARRAY:
            fprintf(output, "ParamArray: %s %s[]", type_name(node->type), node->name);
            break;
        case NODE_COMPOUND_STMT:
            fprintf(output, "CompoundStmt (linha %d)", node->line);
            break;
        case NODE_IF_STMT:
            fprintf(output, "IfStmt (linha %d)", node->line);
            break;
        case NODE_WHILE_STMT:
            fprintf(output, "WhileStmt (linha %d)", node->line);
            break;
        case NODE_RETURN_STMT:
            fprintf(output, "ReturnStmt (linha %d)", node->line);
            break;
        case NODE_EXPR_STMT:
            fprintf(output, "ExprStmt (linha %d)", node->line);
            break;
        case NODE_ASSIGN_EXPR:
            fprintf(output, "Assign (=) (linha %d)", node->line);
            break;
        case NODE_OP_EXPR:
            fprintf(output, "Op: %s (linha %d)", node->name, node->line);
            break;
        case NODE_VAR_EXPR:
            fprintf(output, "Id: %s (linha %d)", node->name, node->line);
            break;
        case NODE_ARRAY_EXPR:
            fprintf(output, "ArrayAccess: %s (linha %d)", node->name, node->line);
            break;
        case NODE_CALL_EXPR:
            fprintf(output, "Call: %s (linha %d)", node->name, node->line);
            break;
        case NODE_CONST_EXPR:
            fprintf(output, "Const: %d (linha %d)", node->value, node->line);
            break;
    }
}

static size_t child_forest_count(const ASTNode *node) {
    size_t total = 0;
    size_t i;
    for (i = 0; i < node->child_count; ++i) {
        const ASTNode *child = node->children[i];
        while (child) {
            ++total;
            child = child->sibling;
        }
    }
    return total;
}

int getHeight(ASTNode *root) {
    int height = 0;
    ASTNode *node;
    if (!root) return 0;
    for (node = root; node; node = node->sibling) {
        int child_height = 0;
        size_t i;
        for (i = 0; i < node->child_count; ++i) {
            int current_height = getHeight(node->children[i]);
            if (current_height > child_height) child_height = current_height;
        }
        if (child_height + 1 > height) height = child_height + 1;
    }
    return height;
}

static FILE *tree_output;
static int *ancestor_is_last;
static size_t ancestor_capacity;

static void reserve_ancestors(size_t required) {
    int *resized;
    size_t capacity;
    size_t i;
    if (required <= ancestor_capacity) return;
    capacity = ancestor_capacity ? ancestor_capacity * 2 : 16;
    while (capacity < required) capacity *= 2;
    resized = (int *)realloc(ancestor_is_last, capacity * sizeof(int));
    if (!resized) allocation_failure();
    for (i = ancestor_capacity; i < capacity; ++i) resized[i] = 1;
    ancestor_is_last = resized;
    ancestor_capacity = capacity;
}

void fillBuffer(ASTNode *node, int level, int has_parent, int is_last) {
    int ancestor;
    size_t i;
    size_t child_count;
    size_t child_index = 0;
    if (!node || level < 0) return;
    if (!tree_output) tree_output = stdout;
    if (level > 0) reserve_ancestors((size_t)level);

    for (ancestor = 0; ancestor < level - 1; ++ancestor) {
        fputs(ancestor_is_last[ancestor] ? "    " : "|   ", tree_output);
    }
    if (has_parent) fputs(is_last ? "`-- " : "|-- ", tree_output);
    print_node_label(tree_output, node);
    fputc('\n', tree_output);

    if (level > 0) {
        ancestor_is_last[level - 1] = is_last != 0;
    }

    child_count = child_forest_count(node);
    for (i = 0; i < node->child_count; ++i) {
        ASTNode *child = node->children[i];
        while (child) {
            ASTNode *next = child->sibling;
            ++child_index;
            fillBuffer(child, level + 1, 1, child_index == child_count);
            child = next;
        }
    }
}

void print_tree(FILE *output, ASTNode *root) {
    ASTNode *node;
    size_t count = 0;
    size_t index = 0;
    if (!output || !root) return;
    tree_output = output;
    fputs("Program\n", output);
    for (node = root; node; node = node->sibling) ++count;
    for (node = root; node; node = node->sibling) {
        ++index;
        fillBuffer(node, 1, 1, index == count);
    }
    free(ancestor_is_last);
    ancestor_is_last = NULL;
    ancestor_capacity = 0;
    tree_output = NULL;
}

void printTree(ASTNode *root) {
    print_tree(stdout, root);
}

static void print_ast_text_node(FILE *output, ASTNode *node, int indent) {
    while (node) {
        size_t i;
        int spaces;
        for (spaces = 0; spaces < indent; ++spaces) fputs("  ", output);
        print_node_label(output, node);
        fputc('\n', output);
        for (i = 0; i < node->child_count; ++i) {
            print_ast_text_node(output, node->children[i], indent + 1);
        }
        node = node->sibling;
    }
}

void print_ast_text(ASTNode *root, int indent) {
    print_ast_text_node(stdout, root, indent);
}

static void dot_escape(FILE *output, const char *text) {
    const unsigned char *character = (const unsigned char *)text;
    while (*character) {
        if (*character == '"' || *character == '\\') fputc('\\', output);
        if (*character == '\n') fputs("\\n", output);
        else fputc(*character, output);
        ++character;
    }
}

static void write_dot_label(FILE *output, const ASTNode *node) {
    switch (node->kind) {
        case NODE_VAR_DECL:
            fprintf(output, "VarDecl\\n%s ", type_name(node->type));
            dot_escape(output, node->name);
            break;
        case NODE_ARRAY_DECL:
            fprintf(output, "ArrayDecl\\n%s ", type_name(node->type));
            dot_escape(output, node->name);
            fprintf(output, "[%d]", node->value);
            break;
        case NODE_FUN_DECL:
            fprintf(output, "FunDecl\\n%s ", type_name(node->type));
            dot_escape(output, node->name);
            break;
        case NODE_PARAM_VAR:
            fprintf(output, "Param\\n%s ", type_name(node->type));
            dot_escape(output, node->name);
            break;
        case NODE_PARAM_ARRAY:
            fprintf(output, "ParamArray\\n%s ", type_name(node->type));
            dot_escape(output, node->name);
            fputs("[]", output);
            break;
        case NODE_COMPOUND_STMT: fputs("CompoundStmt", output); break;
        case NODE_IF_STMT: fputs("IfStmt", output); break;
        case NODE_WHILE_STMT: fputs("WhileStmt", output); break;
        case NODE_RETURN_STMT: fputs("ReturnStmt", output); break;
        case NODE_EXPR_STMT: fputs("ExprStmt", output); break;
        case NODE_ASSIGN_EXPR: fputs("Assign (=)", output); break;
        case NODE_OP_EXPR:
            fputs("Op (", output);
            dot_escape(output, node->name);
            fputc(')', output);
            break;
        case NODE_VAR_EXPR:
            fputs("Id: ", output);
            dot_escape(output, node->name);
            break;
        case NODE_ARRAY_EXPR:
            fputs("Array: ", output);
            dot_escape(output, node->name);
            break;
        case NODE_CALL_EXPR:
            fputs("Call: ", output);
            dot_escape(output, node->name);
            break;
        case NODE_CONST_EXPR:
            fprintf(output, "Const: %d", node->value);
            break;
    }
}

static int write_dot_node(FILE *output, ASTNode *node, int *counter) {
    int current_id;
    int i;
    if (!node) return -1;
    current_id = (*counter)++;
    fprintf(output, "  node%d [label=\"", current_id);
    write_dot_label(output, node);
    fputs("\"];\n", output);

    for (i = 0; i < (int)node->child_count; ++i) {
        ASTNode *child = node->children[i];
        if (child) {
            int child_id = write_dot_node(output, child, counter);
            fprintf(output, "  node%d -> node%d;\n", current_id, child_id);
        }
    }

    if (node->sibling) {
        int sibling_id = write_dot_node(output, node->sibling, counter);
        fprintf(output,
                "  node%d -> node%d [style=dashed, label=\"next\"];\n",
                current_id, sibling_id);
    }
    return current_id;
}

int export_ast_dot(ASTNode *root, const char *filename) {
    FILE *output;
    int counter = 0;
    if (!filename) return 0;
    output = fopen(filename, "w");
    if (!output) return 0;
    fputs("digraph AST {\n  node [shape=box, fontname=\"Arial\"];\n", output);
    if (root) write_dot_node(output, root, &counter);
    fputs("}\n", output);
    if (fclose(output) != 0) return 0;
    return 1;
}
