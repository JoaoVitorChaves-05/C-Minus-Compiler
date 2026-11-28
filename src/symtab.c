#include "symtab.h"

#include <stdlib.h>
#include <string.h>

static char *duplicate_string(const char *text) {
    size_t length;
    char *copy;
    if (!text) return NULL;
    length = strlen(text) + 1;
    copy = (char *)malloc(length);
    if (copy) memcpy(copy, text, length);
    return copy;
}

static int reserve_scopes(SymbolTable *table, size_t required) {
    Scope **scopes;
    size_t capacity;
    if (required <= table->scope_capacity) return 1;
    capacity = table->scope_capacity ? table->scope_capacity * 2 : 8;
    while (capacity < required) capacity *= 2;
    scopes = (Scope **)realloc(table->scopes, capacity * sizeof(Scope *));
    if (!scopes) return 0;
    table->scopes = scopes;
    table->scope_capacity = capacity;
    return 1;
}

static int reserve_symbols(Scope *scope, size_t required) {
    Symbol *symbols;
    size_t capacity;
    if (required <= scope->symbol_capacity) return 1;
    capacity = scope->symbol_capacity ? scope->symbol_capacity * 2 : 8;
    while (capacity < required) capacity *= 2;
    symbols = (Symbol *)realloc(scope->symbols, capacity * sizeof(Symbol));
    if (!symbols) return 0;
    scope->symbols = symbols;
    scope->symbol_capacity = capacity;
    return 1;
}

static void free_symbol(Symbol *symbol) {
    free(symbol->name);
    free(symbol->parameters);
}

static Scope *create_scope(const char *name, Scope *parent) {
    Scope *scope = (Scope *)calloc(1, sizeof(Scope));
    if (!scope) return NULL;
    scope->name = duplicate_string(name);
    if (!scope->name) {
        free(scope);
        return NULL;
    }
    scope->parent = parent;
    return scope;
}

int symbol_table_init(SymbolTable *table) {
    if (!table) return 0;
    memset(table, 0, sizeof(*table));
    table->global = symbol_table_enter_scope(table, "global");
    if (!table->global) {
        symbol_table_destroy(table);
        return 0;
    }
    return 1;
}

void symbol_table_destroy(SymbolTable *table) {
    size_t scope_index;
    if (!table) return;
    for (scope_index = 0; scope_index < table->scope_count; ++scope_index) {
        Scope *scope = table->scopes[scope_index];
        size_t symbol_index;
        for (symbol_index = 0; symbol_index < scope->symbol_count; ++symbol_index) {
            free_symbol(&scope->symbols[symbol_index]);
        }
        free(scope->symbols);
        free(scope->name);
        free(scope);
    }
    free(table->scopes);
    memset(table, 0, sizeof(*table));
}

Scope *symbol_table_enter_scope(SymbolTable *table, const char *name) {
    Scope *scope;
    if (!table || !name || !reserve_scopes(table, table->scope_count + 1)) return NULL;
    scope = create_scope(name, table->current);
    if (!scope) return NULL;
    table->scopes[table->scope_count++] = scope;
    table->current = scope;
    if (!table->global) table->global = scope;
    return scope;
}

int symbol_table_exit_scope(SymbolTable *table) {
    if (!table || !table->current || !table->current->parent) return 0;
    table->current = table->current->parent;
    return 1;
}

SymbolTableResult symbol_table_declare(SymbolTable *table, const Symbol *symbol) {
    Scope *scope;
    Symbol copy;
    size_t index;
    if (!table || !table->current || !symbol || !symbol->name || !symbol->name[0]) {
        return SYMBOL_TABLE_ERROR;
    }
    if (symbol->parameter_count > 0 && !symbol->parameters) {
        return SYMBOL_TABLE_ERROR;
    }

    scope = table->current;
    for (index = 0; index < scope->symbol_count; ++index) {
        if (strcmp(scope->symbols[index].name, symbol->name) == 0) {
            return SYMBOL_DUPLICATE;
        }
    }

    memset(&copy, 0, sizeof(copy));
    copy = *symbol;
    copy.name = duplicate_string(symbol->name);
    copy.parameters = NULL;
    if (!copy.name) return SYMBOL_TABLE_ERROR;

    if (symbol->parameter_count > 0) {
        copy.parameters = (ParameterInfo *)malloc(
            symbol->parameter_count * sizeof(ParameterInfo)
        );
        if (!copy.parameters) {
            free(copy.name);
            return SYMBOL_TABLE_ERROR;
        }
        memcpy(
            copy.parameters,
            symbol->parameters,
            symbol->parameter_count * sizeof(ParameterInfo)
        );
    }

    if (!reserve_symbols(scope, scope->symbol_count + 1)) {
        free_symbol(&copy);
        return SYMBOL_TABLE_ERROR;
    }
    scope->symbols[scope->symbol_count++] = copy;
    return SYMBOL_INSERTED;
}

const Symbol *symbol_table_lookup_current(const SymbolTable *table, const char *name) {
    size_t index;
    if (!table || !table->current || !name) return NULL;
    for (index = 0; index < table->current->symbol_count; ++index) {
        if (strcmp(table->current->symbols[index].name, name) == 0) {
            return &table->current->symbols[index];
        }
    }
    return NULL;
}

const Symbol *symbol_table_lookup(const SymbolTable *table, const char *name) {
    const Scope *scope;
    if (!table || !name) return NULL;
    for (scope = table->current; scope; scope = scope->parent) {
        size_t index;
        for (index = 0; index < scope->symbol_count; ++index) {
            if (strcmp(scope->symbols[index].name, name) == 0) {
                return &scope->symbols[index];
            }
        }
    }
    return NULL;
}

const Scope *symbol_table_current_scope(const SymbolTable *table) {
    return table ? table->current : NULL;
}

static const char *type_name(DataType type) {
    return type == DATA_INT ? "int" : "void";
}

static const char *kind_name(SymbolKind kind) {
    switch (kind) {
        case SYMBOL_VARIABLE: return "variavel";
        case SYMBOL_ARRAY: return "vetor";
        case SYMBOL_FUNCTION: return "funcao";
        case SYMBOL_PARAMETER: return "parametro";
        case SYMBOL_ARRAY_PARAMETER: return "parametro vetor";
    }
    return "desconhecido";
}

static int compare_symbol_pointers(const void *left, const void *right) {
    const Symbol *const *left_symbol = (const Symbol *const *)left;
    const Symbol *const *right_symbol = (const Symbol *const *)right;
    return strcmp((*left_symbol)->name, (*right_symbol)->name);
}

int symbol_table_print(const SymbolTable *table, FILE *output) {
    size_t scope_index;
    if (!table || !output) return 0;

    for (scope_index = 0; scope_index < table->scope_count; ++scope_index) {
        const Scope *scope = table->scopes[scope_index];
        const Symbol **symbols = NULL;
        size_t index;
        fprintf(output, "Escopo: %s\n", scope->name);
        if (scope->symbol_count > 0) {
            symbols = (const Symbol **)malloc(scope->symbol_count * sizeof(Symbol *));
            if (!symbols) return 0;
            for (index = 0; index < scope->symbol_count; ++index) {
                symbols[index] = &scope->symbols[index];
            }
            qsort(symbols, scope->symbol_count, sizeof(Symbol *), compare_symbol_pointers);
        }

        for (index = 0; index < scope->symbol_count; ++index) {
            const Symbol *symbol = symbols[index];
            size_t parameter_index;
            fprintf(output, "  %s : %s, tipo %s",
                    symbol->name, kind_name(symbol->kind), type_name(symbol->type));
            if (symbol->kind == SYMBOL_ARRAY) {
                fprintf(output, ", tamanho %d", symbol->array_size);
            }
            if (symbol->kind == SYMBOL_FUNCTION) {
                fputs(", parametros (", output);
                for (parameter_index = 0;
                     parameter_index < symbol->parameter_count;
                     ++parameter_index) {
                    if (parameter_index > 0) fputs(", ", output);
                    fputs(type_name(symbol->parameters[parameter_index].type), output);
                    if (symbol->parameters[parameter_index].is_array) fputs("[]", output);
                }
                fputc(')', output);
            }
            if (symbol->line > 0) fprintf(output, ", linha %d", symbol->line);
            fputc('\n', output);
        }
        free(symbols);
    }
    return !ferror(output);
}
