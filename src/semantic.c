#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    DataType type;
    int is_array;
    int is_lvalue;
    int valid;
} ExpressionInfo;

typedef struct {
    SymbolTable *symbols;
    int valid;
    char *current_function;
    DataType current_return_type;
    unsigned int next_block_id;
} SemanticContext;

static ASTNode *child(ASTNode *node, size_t index) {
    return ast_child(node, index);
}

static void report_error(SemanticContext *context, const char *identifier, int line) {
    const char *name = identifier;
    if (!name || !name[0]) {
        name = context->current_function && context->current_function[0]
            ? context->current_function
            : "main";
    }
    context->valid = 0;
    fprintf(stderr, "ERRO SEMANTICO: identificador '%s' LINHA: %d\n", name, line);
}

static void report_internal_error(SemanticContext *context) {
    fputs("Erro interno: memoria insuficiente na analise semantica.\n", stderr);
    context->valid = 0;
}

static Symbol create_symbol(const ASTNode *node, SymbolKind kind) {
    Symbol symbol;
    memset(&symbol, 0, sizeof(symbol));
    symbol.name = node->name;
    symbol.kind = kind;
    symbol.type = node->type;
    symbol.line = node->line;
    symbol.array_size = kind == SYMBOL_ARRAY ? node->value : 0;
    return symbol;
}

static int declare_symbol(SemanticContext *context, const Symbol *symbol, int line) {
    SymbolTableResult result = symbol_table_declare(context->symbols, symbol);
    if (result == SYMBOL_DUPLICATE) {
        report_error(context, symbol->name, line);
        return 0;
    }
    if (result == SYMBOL_TABLE_ERROR) {
        report_internal_error(context);
        return 0;
    }
    return 1;
}

static int is_array_kind(SymbolKind kind) {
    return kind == SYMBOL_ARRAY || kind == SYMBOL_ARRAY_PARAMETER;
}

static const char *first_identifier(const ASTNode *node) {
    size_t index;
    const char *identifier;
    if (!node) return NULL;
    if (node->kind == NODE_VAR_EXPR ||
        node->kind == NODE_ARRAY_EXPR ||
        node->kind == NODE_CALL_EXPR) {
        return node->name;
    }
    for (index = 0; index < node->child_count; ++index) {
        identifier = first_identifier(node->children[index]);
        if (identifier) return identifier;
    }
    return node->sibling ? first_identifier(node->sibling) : NULL;
}

static const char *identifier_for(
    const SemanticContext *context,
    const ASTNode *node
) {
    const char *identifier = first_identifier(node);
    if (identifier) return identifier;
    return context->current_function;
}

static void declare_builtins(SemanticContext *context) {
    Symbol input;
    Symbol output;
    ParameterInfo output_parameter;

    memset(&input, 0, sizeof(input));
    input.name = "input";
    input.kind = SYMBOL_FUNCTION;
    input.type = DATA_INT;
    declare_symbol(context, &input, 0);

    memset(&output, 0, sizeof(output));
    output_parameter.type = DATA_INT;
    output_parameter.is_array = 0;
    output.name = "output";
    output.kind = SYMBOL_FUNCTION;
    output.type = DATA_VOID;
    output.parameters = &output_parameter;
    output.parameter_count = 1;
    declare_symbol(context, &output, 0);
}

static void declare_global(SemanticContext *context, ASTNode *node) {
    Symbol symbol;
    SymbolKind kind;
    ASTNode *parameter;
    size_t parameter_count = 0;
    size_t index = 0;

    if (node->kind == NODE_VAR_DECL) {
        kind = SYMBOL_VARIABLE;
    } else if (node->kind == NODE_ARRAY_DECL) {
        kind = SYMBOL_ARRAY;
    } else if (node->kind == NODE_FUN_DECL) {
        kind = SYMBOL_FUNCTION;
        for (parameter = child(node, 0); parameter; parameter = parameter->sibling) {
            ++parameter_count;
        }
    } else {
        return;
    }

    symbol = create_symbol(node, kind);
    if (node->kind == NODE_FUN_DECL && parameter_count > 0) {
        symbol.parameters = (ParameterInfo *)calloc(
            parameter_count, sizeof(ParameterInfo)
        );
        if (!symbol.parameters) {
            report_internal_error(context);
            return;
        }
        symbol.parameter_count = parameter_count;
        for (parameter = child(node, 0);
             parameter;
             parameter = parameter->sibling) {
            symbol.parameters[index].type = parameter->type;
            symbol.parameters[index].is_array =
                parameter->kind == NODE_PARAM_ARRAY;
            ++index;
        }
    }

    declare_symbol(context, &symbol, node->line);
    free(symbol.parameters);

    if ((node->kind == NODE_VAR_DECL || node->kind == NODE_ARRAY_DECL) &&
        node->type == DATA_VOID) {
        report_error(context, node->name, node->line);
    }
    if (node->kind == NODE_ARRAY_DECL && node->value <= 0) {
        report_error(context, node->name, node->line);
    }
}

static void declare_globals(SemanticContext *context, ASTNode *root) {
    ASTNode *declaration;
    for (declaration = root; declaration; declaration = declaration->sibling) {
        declare_global(context, declaration);
    }
}

static void validate_main(SemanticContext *context, ASTNode *root) {
    ASTNode *main_function = NULL;
    ASTNode *last_declaration = NULL;
    ASTNode *declaration;

    for (declaration = root; declaration; declaration = declaration->sibling) {
        last_declaration = declaration;
        if (declaration->kind == NODE_FUN_DECL &&
            strcmp(declaration->name, "main") == 0) {
            main_function = declaration;
        }
    }

    if (!main_function) {
        report_error(context, "main", root ? root->line : 1);
        return;
    }
    if (last_declaration != main_function) {
        report_error(context, "main", main_function->line);
    }
    if (main_function->type != DATA_VOID || child(main_function, 0) != NULL) {
        report_error(context, "main", main_function->line);
    }
}

static void declare_local(SemanticContext *context, ASTNode *node) {
    SymbolKind kind;
    Symbol symbol;
    if (node->kind == NODE_VAR_DECL) {
        kind = SYMBOL_VARIABLE;
    } else if (node->kind == NODE_ARRAY_DECL) {
        kind = SYMBOL_ARRAY;
    } else {
        return;
    }

    symbol = create_symbol(node, kind);
    declare_symbol(context, &symbol, node->line);
    if (node->type == DATA_VOID) {
        report_error(context, node->name, node->line);
    }
    if (node->kind == NODE_ARRAY_DECL && node->value <= 0) {
        report_error(context, node->name, node->line);
    }
}

static int require_integer_scalar(
    SemanticContext *context,
    ExpressionInfo info,
    ASTNode *node
) {
    if (!info.valid) return 0;
    if (info.type != DATA_INT || info.is_array) {
        report_error(context, identifier_for(context, node), node ? node->line : 0);
        return 0;
    }
    return 1;
}

static ExpressionInfo analyze_expression(SemanticContext *context, ASTNode *expression);
static void analyze_statement(SemanticContext *context, ASTNode *statement);

static void analyze_compound_contents(SemanticContext *context, ASTNode *compound) {
    ASTNode *declaration;
    ASTNode *statement;
    for (declaration = child(compound, 0);
         declaration;
         declaration = declaration->sibling) {
        declare_local(context, declaration);
    }
    for (statement = child(compound, 1);
         statement;
         statement = statement->sibling) {
        analyze_statement(context, statement);
    }
}

static void analyze_function(SemanticContext *context, ASTNode *function) {
    ASTNode *parameter;
    ASTNode *body;
    size_t name_length = strlen(function->name) + 1;
    char *name_copy = (char *)malloc(name_length);

    if (!name_copy) {
        report_internal_error(context);
        return;
    }
    memcpy(name_copy, function->name, name_length);
    free(context->current_function);
    context->current_function = name_copy;
    context->current_return_type = function->type;

    if (!symbol_table_enter_scope(context->symbols, function->name)) {
        report_internal_error(context);
        return;
    }

    for (parameter = child(function, 0);
         parameter;
         parameter = parameter->sibling) {
        SymbolKind kind = parameter->kind == NODE_PARAM_ARRAY
            ? SYMBOL_ARRAY_PARAMETER
            : SYMBOL_PARAMETER;
        Symbol symbol = create_symbol(parameter, kind);
        declare_symbol(context, &symbol, parameter->line);
        if (parameter->type == DATA_VOID) {
            report_error(context, parameter->name, parameter->line);
        }
    }

    body = child(function, 1);
    if (body && body->kind == NODE_COMPOUND_STMT) {
        analyze_compound_contents(context, body);
    }
    symbol_table_exit_scope(context->symbols);
}

static void analyze_return(SemanticContext *context, ASTNode *statement) {
    ASTNode *value = child(statement, 0);
    if (context->current_return_type == DATA_VOID) {
        if (value) {
            analyze_expression(context, value);
            report_error(context, context->current_function, statement->line);
        }
        return;
    }
    if (!value) {
        report_error(context, context->current_function, statement->line);
        return;
    }
    require_integer_scalar(context, analyze_expression(context, value), value);
}

static void analyze_statement(SemanticContext *context, ASTNode *statement) {
    ASTNode *condition;
    char block_name[64];
    if (!statement) return;

    switch (statement->kind) {
        case NODE_EXPR_STMT:
            analyze_expression(context, child(statement, 0));
            break;
        case NODE_COMPOUND_STMT:
            snprintf(block_name, sizeof(block_name), "%s::block%u",
                     context->current_function, context->next_block_id++);
            if (!symbol_table_enter_scope(context->symbols, block_name)) {
                report_internal_error(context);
                break;
            }
            analyze_compound_contents(context, statement);
            symbol_table_exit_scope(context->symbols);
            break;
        case NODE_IF_STMT:
            condition = child(statement, 0);
            require_integer_scalar(
                context, analyze_expression(context, condition), condition
            );
            analyze_statement(context, child(statement, 1));
            analyze_statement(context, child(statement, 2));
            break;
        case NODE_WHILE_STMT:
            condition = child(statement, 0);
            require_integer_scalar(
                context, analyze_expression(context, condition), condition
            );
            analyze_statement(context, child(statement, 1));
            break;
        case NODE_RETURN_STMT:
            analyze_return(context, statement);
            break;
        default:
            break;
    }
}

static ExpressionInfo invalid_expression(void) {
    ExpressionInfo info = {DATA_INT, 0, 0, 0};
    return info;
}

static ExpressionInfo analyze_variable(SemanticContext *context, ASTNode *expression) {
    const Symbol *symbol = symbol_table_lookup(context->symbols, expression->name);
    if (!symbol) {
        report_error(context, expression->name, expression->line);
        return invalid_expression();
    }

    if (symbol->kind == SYMBOL_VARIABLE || symbol->kind == SYMBOL_PARAMETER) {
        ExpressionInfo info = {symbol->type, 0, 1, 1};
        return info;
    }
    if (is_array_kind(symbol->kind)) {
        ExpressionInfo info = {symbol->type, 1, 0, 1};
        return info;
    }
    report_error(context, expression->name, expression->line);
    return invalid_expression();
}

static ExpressionInfo analyze_array_access(
    SemanticContext *context,
    ASTNode *expression
) {
    const Symbol *symbol = symbol_table_lookup(context->symbols, expression->name);
    ASTNode *index_node = child(expression, 0);
    ExpressionInfo index = analyze_expression(context, index_node);
    int index_valid = require_integer_scalar(context, index, index_node);

    if (!symbol) {
        report_error(context, expression->name, expression->line);
        return invalid_expression();
    }
    if (!is_array_kind(symbol->kind)) {
        report_error(context, expression->name, expression->line);
        return invalid_expression();
    }

    {
        ExpressionInfo info = {
            symbol->type,
            0,
            1,
            index_valid && symbol->type == DATA_INT
        };
        return info;
    }
}

static ExpressionInfo analyze_call(SemanticContext *context, ASTNode *call) {
    const Symbol *function = symbol_table_lookup(context->symbols, call->name);
    ASTNode *argument;
    size_t argument_index = 0;
    int valid = 1;

    if (!function) {
        for (argument = child(call, 0); argument; argument = argument->sibling) {
            analyze_expression(context, argument);
        }
        report_error(context, call->name, call->line);
        return invalid_expression();
    }
    if (function->kind != SYMBOL_FUNCTION) {
        for (argument = child(call, 0); argument; argument = argument->sibling) {
            analyze_expression(context, argument);
        }
        report_error(context, call->name, call->line);
        return invalid_expression();
    }

    for (argument = child(call, 0); argument; argument = argument->sibling) {
        ExpressionInfo actual = analyze_expression(context, argument);
        if (argument_index < function->parameter_count && actual.valid) {
            const ParameterInfo *expected = &function->parameters[argument_index];
            if (actual.type != expected->type ||
                actual.is_array != expected->is_array) {
                report_error(
                    context,
                    identifier_for(context, argument),
                    argument->line
                );
                valid = 0;
            }
        } else if (!actual.valid) {
            valid = 0;
        }
        ++argument_index;
    }

    if (argument_index != function->parameter_count) {
        report_error(context, call->name, call->line);
        valid = 0;
    }

    {
        ExpressionInfo info = {function->type, 0, 0, valid};
        return info;
    }
}

static ExpressionInfo analyze_expression(
    SemanticContext *context,
    ASTNode *expression
) {
    if (!expression) return invalid_expression();

    switch (expression->kind) {
        case NODE_CONST_EXPR: {
            ExpressionInfo info = {DATA_INT, 0, 0, 1};
            return info;
        }
        case NODE_VAR_EXPR:
            return analyze_variable(context, expression);
        case NODE_ARRAY_EXPR:
            return analyze_array_access(context, expression);
        case NODE_CALL_EXPR:
            return analyze_call(context, expression);
        case NODE_OP_EXPR: {
            ASTNode *left_node = child(expression, 0);
            ASTNode *right_node = child(expression, 1);
            ExpressionInfo left = analyze_expression(context, left_node);
            ExpressionInfo right = analyze_expression(context, right_node);
            int left_valid = require_integer_scalar(
                context, left, left_node ? left_node : expression
            );
            int right_valid = require_integer_scalar(
                context, right, right_node ? right_node : expression
            );
            ExpressionInfo info = {DATA_INT, 0, 0, left_valid && right_valid};
            return info;
        }
        case NODE_ASSIGN_EXPR: {
            ASTNode *left_node = child(expression, 0);
            ASTNode *right_node = child(expression, 1);
            ExpressionInfo left = analyze_expression(context, left_node);
            ExpressionInfo right = analyze_expression(context, right_node);
            int valid = left.valid && right.valid;
            if (left.valid && (!left.is_lvalue || left.is_array)) {
                report_error(
                    context,
                    identifier_for(context, left_node),
                    left_node ? left_node->line : expression->line
                );
                valid = 0;
            }
            if (right.valid && (right.type != DATA_INT || right.is_array)) {
                report_error(
                    context,
                    identifier_for(context, right_node),
                    right_node ? right_node->line : expression->line
                );
                valid = 0;
            }
            {
                ExpressionInfo info = {left.type, 0, 0, valid};
                return info;
            }
        }
        default:
            return invalid_expression();
    }
}

int analyze_semantics(ASTNode *root, SymbolTable *symbols) {
    SemanticContext context;
    ASTNode *declaration;
    memset(&context, 0, sizeof(context));
    context.symbols = symbols;
    context.valid = 1;
    context.current_return_type = DATA_VOID;
    context.next_block_id = 1;

    if (!symbols || !symbol_table_current_scope(symbols)) {
        fputs("Erro interno: tabela de simbolos nao inicializada.\n", stderr);
        return 0;
    }

    declare_builtins(&context);
    declare_globals(&context, root);
    validate_main(&context, root);

    for (declaration = root; declaration; declaration = declaration->sibling) {
        if (declaration->kind == NODE_FUN_DECL) {
            analyze_function(&context, declaration);
        }
    }

    free(context.current_function);
    return context.valid;
}
