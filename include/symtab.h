#ifndef SYMTAB_H
#define SYMTAB_H

#include "ast.h"

#include <stddef.h>
#include <stdio.h>

typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_ARRAY,
    SYMBOL_FUNCTION,
    SYMBOL_PARAMETER,
    SYMBOL_ARRAY_PARAMETER
} SymbolKind;

typedef struct {
    DataType type;
    int is_array;
} ParameterInfo;

typedef struct {
    char *name;
    SymbolKind kind;
    DataType type;
    int line;
    int array_size;
    ParameterInfo *parameters;
    size_t parameter_count;
} Symbol;

typedef struct Scope {
    char *name;
    struct Scope *parent;
    Symbol *symbols;
    size_t symbol_count;
    size_t symbol_capacity;
} Scope;

typedef struct {
    Scope *global;
    Scope *current;
    Scope **scopes;
    size_t scope_count;
    size_t scope_capacity;
} SymbolTable;

typedef enum {
    SYMBOL_TABLE_ERROR = -1,
    SYMBOL_DUPLICATE = 0,
    SYMBOL_INSERTED = 1
} SymbolTableResult;

int symbol_table_init(SymbolTable *table);
void symbol_table_destroy(SymbolTable *table);
Scope *symbol_table_enter_scope(SymbolTable *table, const char *name);
int symbol_table_exit_scope(SymbolTable *table);
SymbolTableResult symbol_table_declare(SymbolTable *table, const Symbol *symbol);
const Symbol *symbol_table_lookup(const SymbolTable *table, const char *name);
const Symbol *symbol_table_lookup_current(const SymbolTable *table, const char *name);
const Scope *symbol_table_current_scope(const SymbolTable *table);
int symbol_table_print(const SymbolTable *table, FILE *output);

#endif
