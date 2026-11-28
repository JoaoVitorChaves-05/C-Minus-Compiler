#include "gci.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *source_name;
    char *ir_name;
    int is_array;
} Binding;

typedef struct {
    GCI *output;
    ASTNode *program;
    Binding *bindings;
    size_t binding_count;
    size_t binding_capacity;
    size_t next_symbol;
    size_t next_temp;
    size_t next_label;
    int failed;
} Generator;

typedef struct {
    const char *name;
    char *index;
    int is_array;
} LValue;

static char *copy_string(const char *text) {
    size_t length;
    char *copy;
    if (!text) return NULL;
    length = strlen(text) + 1;
    copy = (char *)malloc(length);
    if (!copy) return NULL;
    memcpy(copy, text, length);
    return copy;
}

static void pop_bindings(Generator *generator, size_t count) {
    while (generator->binding_count > count) {
        free(generator->bindings[--generator->binding_count].ir_name);
    }
}

static int add_binding(
    Generator *generator,
    const char *source_name,
    int is_array,
    const char **ir_name
) {
    Binding *resized;
    Binding *binding;
    size_t capacity;
    char buffer[48];

    if (generator->binding_count == generator->binding_capacity) {
        if (generator->binding_capacity > (size_t)-1 / 2) {
            generator->failed = 1;
            return 0;
        }
        capacity = generator->binding_capacity
            ? generator->binding_capacity * 2
            : 16;
        if (capacity > (size_t)-1 / sizeof(Binding)) {
            generator->failed = 1;
            return 0;
        }
        resized = (Binding *)realloc(
            generator->bindings, capacity * sizeof(Binding)
        );
        if (!resized) {
            generator->failed = 1;
            return 0;
        }
        generator->bindings = resized;
        generator->binding_capacity = capacity;
    }

    snprintf(buffer, sizeof(buffer), "$v%zu", generator->next_symbol++);
    binding = &generator->bindings[generator->binding_count];
    binding->source_name = source_name;
    binding->ir_name = copy_string(buffer);
    binding->is_array = is_array;
    if (!binding->ir_name) {
        generator->failed = 1;
        return 0;
    }
    ++generator->binding_count;
    *ir_name = binding->ir_name;
    return 1;
}

static const Binding *find_binding(
    const Generator *generator,
    const char *source_name
) {
    size_t index = generator->binding_count;
    while (index > 0) {
        const Binding *binding = &generator->bindings[--index];
        if (strcmp(binding->source_name, source_name) == 0) return binding;
    }
    return NULL;
}

static const char *resolve_name(
    Generator *generator,
    const char *source_name
) {
    const Binding *binding = find_binding(generator, source_name);
    if (!binding) {
        generator->failed = 1;
        return NULL;
    }
    return binding->ir_name;
}

static char *array_address_operand(
    Generator *generator,
    const char *source_name
) {
    const Binding *binding = find_binding(generator, source_name);
    char *address;
    size_t name_length;
    if (!binding) {
        generator->failed = 1;
        return NULL;
    }
    if (!binding->is_array) return copy_string(binding->ir_name);
    name_length = strlen(binding->ir_name);
    address = (char *)malloc(name_length + 2);
    if (!address) {
        generator->failed = 1;
        return NULL;
    }
    address[0] = '&';
    memcpy(address + 1, binding->ir_name, name_length + 1);
    return address;
}

static void free_quad(Quad *quad) {
    free(quad->op);
    free(quad->arg1);
    free(quad->arg2);
    free(quad->res);
    memset(quad, 0, sizeof(*quad));
}

void clear_quads(GCI *gci) {
    size_t index;
    if (!gci) return;
    for (index = 0; index < gci->count; ++index) {
        free_quad(&gci->quads[index]);
    }
    free(gci->quads);
    memset(gci, 0, sizeof(*gci));
}

static int reserve_quads(GCI *gci, size_t required) {
    Quad *resized;
    size_t capacity;
    if (required <= gci->capacity) return 1;
    if (gci->capacity) {
        if (gci->capacity > (size_t)-1 / 2) return 0;
        capacity = gci->capacity * 2;
    } else {
        capacity = 32;
    }
    while (capacity < required) {
        if (capacity > (size_t)-1 / 2) return 0;
        capacity *= 2;
    }
    if (capacity > (size_t)-1 / sizeof(Quad)) return 0;
    resized = (Quad *)realloc(gci->quads, capacity * sizeof(Quad));
    if (!resized) return 0;
    gci->quads = resized;
    gci->capacity = capacity;
    return 1;
}

static int emit(
    Generator *generator,
    const char *op,
    const char *arg1,
    const char *arg2,
    const char *result
) {
    Quad quad;
    GCI *gci = generator->output;
    memset(&quad, 0, sizeof(quad));
    if (!reserve_quads(gci, gci->count + 1)) {
        generator->failed = 1;
        return 0;
    }

    quad.op = copy_string(op ? op : "-");
    quad.arg1 = copy_string(arg1 ? arg1 : "-");
    quad.arg2 = copy_string(arg2 ? arg2 : "-");
    quad.res = copy_string(result ? result : "-");
    if (!quad.op || !quad.arg1 || !quad.arg2 || !quad.res) {
        free_quad(&quad);
        generator->failed = 1;
        return 0;
    }

    gci->quads[gci->count++] = quad;
    return 1;
}

static char *new_temp(Generator *generator) {
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "$t%zu", generator->next_temp++);
    return copy_string(buffer);
}

static char *new_label(Generator *generator) {
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "L%zu", generator->next_label++);
    return copy_string(buffer);
}

static int emit_label(Generator *generator, const char *label) {
    return emit(generator, "LAB", label, "-", "-");
}

static int function_returns_int(const Generator *generator, const char *name) {
    const ASTNode *declaration;
    if (strcmp(name, "input") == 0) return 1;
    if (strcmp(name, "output") == 0) return 0;
    for (declaration = generator->program; declaration;
         declaration = declaration->sibling) {
        if (declaration->kind == NODE_FUN_DECL && declaration->name &&
            strcmp(declaration->name, name) == 0) {
            return declaration->type == DATA_INT;
        }
    }
    return 0;
}

static char *generate_expression(Generator *generator, ASTNode *node);

static int generate_lvalue(
    Generator *generator,
    ASTNode *node,
    LValue *lvalue
) {
    memset(lvalue, 0, sizeof(*lvalue));
    if (!node) {
        generator->failed = 1;
        return 0;
    }

    if (node->kind == NODE_VAR_EXPR && node->name) {
        lvalue->name = resolve_name(generator, node->name);
        return lvalue->name != NULL;
    }

    if (node->kind == NODE_ARRAY_EXPR && node->name) {
        lvalue->name = resolve_name(generator, node->name);
        if (!lvalue->name) return 0;
        lvalue->index = generate_expression(generator, ast_child(node, 0));
        if (!lvalue->index || generator->failed) {
            free(lvalue->index);
            lvalue->index = NULL;
            return 0;
        }
        lvalue->is_array = 1;
        return 1;
    }

    generator->failed = 1;
    return 0;
}

static void clear_lvalue(LValue *lvalue) {
    free(lvalue->index);
    memset(lvalue, 0, sizeof(*lvalue));
}

static char *generate_call(Generator *generator, ASTNode *node) {
    ASTNode *argument;
    char **arguments = NULL;
    size_t argument_count = 0;
    size_t argument_capacity = 0;
    size_t index;
    char count_text[48];
    char *result = NULL;

    if (!node->name) {
        generator->failed = 1;
        return NULL;
    }

    for (argument = ast_child(node, 0); argument; argument = argument->sibling) {
        char *value = generate_expression(generator, argument);
        if (!value || generator->failed) {
            free(value);
            goto cleanup;
        }
        if (argument->kind == NODE_VAR_EXPR && value[0] != '&') {
            char *snapshot = new_temp(generator);
            if (!snapshot ||
                !emit(generator, "=", value, "-", snapshot)) {
                free(snapshot);
                free(value);
                generator->failed = 1;
                goto cleanup;
            }
            free(value);
            value = snapshot;
        }
        if (argument_count == argument_capacity) {
            size_t new_capacity;
            char **resized;
            if (argument_capacity > (size_t)-1 / 2) {
                free(value);
                generator->failed = 1;
                goto cleanup;
            }
            new_capacity = argument_capacity ? argument_capacity * 2 : 4;
            if (new_capacity > (size_t)-1 / sizeof(char *)) {
                free(value);
                generator->failed = 1;
                goto cleanup;
            }
            resized = (char **)realloc(arguments, new_capacity * sizeof(char *));
            if (!resized) {
                free(value);
                generator->failed = 1;
                goto cleanup;
            }
            arguments = resized;
            argument_capacity = new_capacity;
        }
        arguments[argument_count++] = value;
    }

    snprintf(count_text, sizeof(count_text), "%zu", argument_count);
    for (index = 0; index < argument_count; ++index) {
        if (!emit(generator, "PARAM", arguments[index], "-", "-")) {
            goto cleanup;
        }
    }

    if (function_returns_int(generator, node->name)) {
        result = new_temp(generator);
        if (!result) {
            generator->failed = 1;
            goto cleanup;
        }
    } else {
        result = copy_string("-");
        if (!result) {
            generator->failed = 1;
            goto cleanup;
        }
    }

    if (!emit(generator, "CALL", node->name, count_text, result)) {
        free(result);
        result = NULL;
        goto cleanup;
    }

cleanup:
    for (index = 0; index < argument_count; ++index) free(arguments[index]);
    free(arguments);
    if (generator->failed) {
        free(result);
        return NULL;
    }
    return result;
}

static char *generate_expression(Generator *generator, ASTNode *node) {
    char *left;
    char *right;
    char *result;
    ASTNode *left_node;
    ASTNode *right_node;

    if (!node) {
        generator->failed = 1;
        return NULL;
    }

    switch (node->kind) {
        case NODE_CONST_EXPR: {
            char buffer[48];
            snprintf(buffer, sizeof(buffer), "%d", node->value);
            result = copy_string(buffer);
            if (!result) generator->failed = 1;
            return result;
        }

        case NODE_VAR_EXPR:
            if (!node->name) {
                generator->failed = 1;
                return NULL;
            }
            return array_address_operand(generator, node->name);

        case NODE_ARRAY_EXPR: {
            char *index = generate_expression(generator, ast_child(node, 0));
            char *temporary;
            const char *array_name = node->name
                ? resolve_name(generator, node->name)
                : NULL;
            if (!index || !array_name || generator->failed) {
                free(index);
                generator->failed = 1;
                return NULL;
            }
            temporary = new_temp(generator);
            if (!temporary ||
                !emit(generator, "[]", array_name, index, temporary)) {
                free(index);
                free(temporary);
                generator->failed = 1;
                return NULL;
            }
            free(index);
            return temporary;
        }

        case NODE_OP_EXPR:
            left_node = ast_child(node, 0);
            right_node = ast_child(node, 1);
            if (!node->name || !left_node || !right_node) {
                generator->failed = 1;
                return NULL;
            }
            left = generate_expression(generator, left_node);
            right = generate_expression(generator, right_node);
            if (!left || !right || generator->failed) {
                free(left);
                free(right);
                return NULL;
            }
            result = new_temp(generator);
            if (!result || !emit(generator, node->name, left, right, result)) {
                free(left);
                free(right);
                free(result);
                generator->failed = 1;
                return NULL;
            }
            free(left);
            free(right);
            return result;

        case NODE_ASSIGN_EXPR: {
            LValue target;
            ASTNode *target_node = ast_child(node, 0);
            char *value;
            if (!generate_lvalue(generator, target_node, &target)) return NULL;
            value = generate_expression(generator, ast_child(node, 1));
            if (!value || generator->failed) {
                clear_lvalue(&target);
                free(value);
                return NULL;
            }
            if (target.is_array) {
                generator->failed = !emit(
                    generator, "[]=", target.name, target.index, value
                );
            } else {
                generator->failed = !emit(
                    generator, "=", value, "-", target.name
                );
            }
            clear_lvalue(&target);
            if (generator->failed) {
                free(value);
                return NULL;
            }
            return value;
        }

        case NODE_CALL_EXPR:
            return generate_call(generator, node);

        default:
            generator->failed = 1;
            return NULL;
    }
}

static int generate_declaration(
    Generator *generator,
    ASTNode *declaration,
    const char *scope,
    int is_global
) {
    char size_text[48];
    const char *ir_name;
    int is_array;
    if (!declaration || !declaration->name) {
        generator->failed = 1;
        return 0;
    }
    if (declaration->kind == NODE_VAR_DECL) {
        is_array = 0;
    } else if (declaration->kind == NODE_ARRAY_DECL) {
        is_array = 1;
    } else {
        generator->failed = 1;
        return 0;
    }

    if (is_global) {
        ir_name = resolve_name(generator, declaration->name);
    } else if (!add_binding(generator, declaration->name, is_array, &ir_name)) {
        return 0;
    }
    if (!ir_name) return 0;

    if (!is_array) {
        return emit(generator, "ALLOC", ir_name, scope, "-");
    } else {
        snprintf(size_text, sizeof(size_text), "%d", declaration->value);
        return emit(
            generator, "ALLOC_ARRAY", ir_name, size_text, scope
        );
    }
}

static int generate_statement(Generator *generator, ASTNode *statement,
                              const char *scope);

static int generate_statement_list(Generator *generator, ASTNode *statement,
                                   const char *scope) {
    for (; statement; statement = statement->sibling) {
        if (!generate_statement(generator, statement, scope)) return 0;
    }
    return 1;
}

static int generate_compound(Generator *generator, ASTNode *compound,
                             const char *scope) {
    ASTNode *declaration;
    size_t binding_mark = generator->binding_count;
    int success = 1;
    if (!compound || compound->kind != NODE_COMPOUND_STMT) {
        generator->failed = 1;
        return 0;
    }

    for (declaration = ast_child(compound, 0); declaration;
         declaration = declaration->sibling) {
        if (!generate_declaration(generator, declaration, scope, 0)) {
            success = 0;
            break;
        }
    }
    if (success) {
        success = generate_statement_list(
            generator, ast_child(compound, 1), scope
        );
    }
    pop_bindings(generator, binding_mark);
    return success;
}

static int generate_statement(Generator *generator, ASTNode *statement,
                              const char *scope) {
    ASTNode *condition;
    ASTNode *body;
    char *value;
    char *false_label;
    char *end_label;

    if (!statement) {
        generator->failed = 1;
        return 0;
    }

    switch (statement->kind) {
        case NODE_EXPR_STMT:
            if (ast_child(statement, 0)) {
                value = generate_expression(generator, ast_child(statement, 0));
                free(value);
            }
            return !generator->failed;

        case NODE_COMPOUND_STMT:
            return generate_compound(generator, statement, scope);

        case NODE_IF_STMT:
            condition = ast_child(statement, 0);
            body = ast_child(statement, 1);
            value = generate_expression(generator, condition);
            false_label = new_label(generator);
            if (!value || !false_label || generator->failed ||
                !emit(generator, "IFF", value, "-", false_label)) {
                free(value);
                free(false_label);
                generator->failed = 1;
                return 0;
            }
            free(value);
            if (!generate_statement(generator, body, scope)) {
                free(false_label);
                return 0;
            }

            if (ast_child(statement, 2)) {
                end_label = new_label(generator);
                if (!end_label ||
                    !emit(generator, "GOTO", end_label, "-", "-") ||
                    !emit_label(generator, false_label)) {
                    free(false_label);
                    free(end_label);
                    generator->failed = 1;
                    return 0;
                }
                free(false_label);
                if (!generate_statement(
                        generator, ast_child(statement, 2), scope)) {
                    free(end_label);
                    return 0;
                }
                if (!emit_label(generator, end_label)) {
                    free(end_label);
                    return 0;
                }
                free(end_label);
                return 1;
            }

            if (!emit_label(generator, false_label)) {
                free(false_label);
                return 0;
            }
            free(false_label);
            return 1;

        case NODE_WHILE_STMT: {
            char *start_label = new_label(generator);
            char *exit_label = new_label(generator);
            condition = ast_child(statement, 0);
            body = ast_child(statement, 1);
            if (!start_label || !exit_label ||
                !emit_label(generator, start_label)) {
                free(start_label);
                free(exit_label);
                generator->failed = 1;
                return 0;
            }
            value = generate_expression(generator, condition);
            if (!value || generator->failed ||
                !emit(generator, "IFF", value, "-", exit_label)) {
                free(start_label);
                free(exit_label);
                free(value);
                generator->failed = 1;
                return 0;
            }
            free(value);
            if (!generate_statement(generator, body, scope) ||
                !emit(generator, "GOTO", start_label, "-", "-") ||
                !emit_label(generator, exit_label)) {
                free(start_label);
                free(exit_label);
                return 0;
            }
            free(start_label);
            free(exit_label);
            return 1;
        }

        case NODE_RETURN_STMT:
            value = ast_child(statement, 0)
                ? generate_expression(generator, ast_child(statement, 0))
                : copy_string("-");
            if (!value) {
                generator->failed = 1;
                return 0;
            }
            generator->failed = !emit(generator, "RET", value, "-", "-");
            free(value);
            return !generator->failed;

        default:
            generator->failed = 1;
            return 0;
    }
}

static int generate_function(Generator *generator, ASTNode *function) {
    ASTNode *parameter;
    ASTNode *body = ast_child(function, 1);
    size_t binding_mark = generator->binding_count;
    int success = 1;
    if (!function->name ||
        !emit(generator,
              "FUN",
              function->type == DATA_INT ? "int" : "void",
              function->name,
              "-")) {
        generator->failed = 1;
        return 0;
    }

    for (parameter = ast_child(function, 0); parameter;
         parameter = parameter->sibling) {
        const char *ir_name;
        int is_array = parameter->kind == NODE_PARAM_ARRAY;
        if (!parameter->name ||
            !add_binding(generator, parameter->name, is_array, &ir_name) ||
            !emit(generator,
                  is_array ? "ARG_ARRAY" : "ARG",
                  parameter->type == DATA_INT ? "int" : "void",
                  ir_name,
                  function->name)) {
            generator->failed = 1;
            success = 0;
            break;
        }
    }

    if (success) {
        success = generate_compound(generator, body, function->name);
    }
    if (success) {
        success = emit(generator, "END", function->name, "-", "-");
    }
    if (success && strcmp(function->name, "main") == 0) {
        success = emit(generator, "HALT", "-", "-", "-");
    }
    pop_bindings(generator, binding_mark);
    return success;
}

int generate_quads(ASTNode *root, GCI *gci) {
    Generator generator;
    ASTNode *declaration;
    if (!gci) return 0;
    clear_quads(gci);
    if (!root) return 0;
    memset(&generator, 0, sizeof(generator));
    generator.output = gci;
    generator.program = root;

    if (root->kind == NODE_CONST_EXPR || root->kind == NODE_VAR_EXPR ||
        root->kind == NODE_ARRAY_EXPR || root->kind == NODE_OP_EXPR ||
        root->kind == NODE_ASSIGN_EXPR || root->kind == NODE_CALL_EXPR) {
        char *value = generate_expression(&generator, root);
        free(value);
        if (generator.failed) {
            clear_quads(gci);
            return 0;
        }
        return 1;
    }

    for (declaration = root; declaration; declaration = declaration->sibling) {
        if (declaration->kind == NODE_VAR_DECL ||
            declaration->kind == NODE_ARRAY_DECL) {
            const char *ir_name;
            if (!declaration->name) {
                generator.failed = 1;
                break;
            }
            if (!add_binding(
                    &generator,
                    declaration->name,
                    declaration->kind == NODE_ARRAY_DECL,
                    &ir_name)) {
                break;
            }
        }
    }
    if (generator.failed) {
        pop_bindings(&generator, 0);
        free(generator.bindings);
        clear_quads(gci);
        return 0;
    }

    for (declaration = root; declaration; declaration = declaration->sibling) {
        int success;
        if (declaration->kind == NODE_FUN_DECL) {
            success = generate_function(&generator, declaration);
        } else {
            success = generate_declaration(
                &generator, declaration, "global", 1
            );
        }
        if (!success || generator.failed) {
            pop_bindings(&generator, 0);
            free(generator.bindings);
            clear_quads(gci);
            return 0;
        }
    }
    pop_bindings(&generator, 0);
    free(generator.bindings);
    return 1;
}

void print_quads(FILE *output, const GCI *gci) {
    size_t index;
    if (!output || !gci) return;
    for (index = 0; index < gci->count; ++index) {
        const Quad *quad = &gci->quads[index];
        fprintf(output, "(%s, %s, %s, %s)\n",
                quad->op, quad->arg1, quad->arg2, quad->res);
    }
}
