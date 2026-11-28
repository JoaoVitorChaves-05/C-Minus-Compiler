#ifndef GCI_H
#define GCI_H

#include "ast.h"

typedef struct {
    char *op;
    char *arg1;
    char *arg2;
    char *res;
} Quad;

typedef struct {
    Quad *quads;
    size_t count;
    size_t capacity;
} GCI;

#define GCI_INITIALIZER { 0 }

int generate_quads(ASTNode *root, GCI *gci);
void print_quads(FILE *output, const GCI *gci);
void clear_quads(GCI *gci);

#endif
