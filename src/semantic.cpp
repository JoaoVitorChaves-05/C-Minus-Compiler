#include "semantic.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct ExpressionInfo {
    DataType type = DataType::INT_TYPE;
    bool isArray = false;
    bool isLValue = false;
    bool valid = true;
};

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(SymbolTable& symbols) : symbols_(symbols) {}

    bool analyze(ASTNode* root) {
        declare_builtins();
        declare_globals(root);
        validate_main(root);

        for (ASTNode* declaration = root; declaration; declaration = declaration->sibling) {
            if (declaration->kind == NodeKind::FUN_DECL) {
                analyze_function(declaration);
            }
        }

        return valid_;
    }

private:
    SymbolTable& symbols_;
    bool valid_ = true;
    std::string current_function_;
    DataType current_return_type_ = DataType::VOID_TYPE;
    unsigned int next_block_id_ = 1;

    void report(const std::string& identifier, int line) {
        valid_ = false;
        const std::string name = identifier.empty() ? "main" : identifier;
        std::cerr << "ERRO SEMANTICO: identificador '" << name
                  << "' LINHA: " << line << '\n';
    }

    static ASTNode* child(ASTNode* node, size_t index) {
        return node && index < node->children.size() ? node->children[index] : nullptr;
    }

    static Symbol make_symbol(
        const ASTNode* node,
        SymbolKind kind
    ) {
        Symbol symbol;
        symbol.name = node->name;
        symbol.kind = kind;
        symbol.type = node->type;
        symbol.line = node->line;
        symbol.arraySize = kind == SymbolKind::Array ? node->value : 0;
        return symbol;
    }

    static bool is_array_kind(SymbolKind kind) {
        return kind == SymbolKind::Array || kind == SymbolKind::ArrayParameter;
    }

    static std::string first_identifier(const ASTNode* node) {
        if (!node) return std::string();
        if (node->kind == NodeKind::VAR_EXPR ||
            node->kind == NodeKind::ARRAY_EXPR ||
            node->kind == NodeKind::CALL_EXPR) {
            return node->name;
        }
        for (const ASTNode* child_node : node->children) {
            const std::string identifier = first_identifier(child_node);
            if (!identifier.empty()) return identifier;
        }
        if (node->sibling) return first_identifier(node->sibling);
        return std::string();
    }

    std::string identifier_for(const ASTNode* node) const {
        const std::string identifier = first_identifier(node);
        return identifier.empty() ? current_function_ : identifier;
    }

    void declare_builtins() {
        Symbol input;
        input.name = "input";
        input.kind = SymbolKind::Function;
        input.type = DataType::INT_TYPE;
        input.line = 0;
        if (!symbols_.declare(input)) report(input.name, input.line);

        Symbol output;
        output.name = "output";
        output.kind = SymbolKind::Function;
        output.type = DataType::VOID_TYPE;
        output.line = 0;
        output.parameters.push_back({DataType::INT_TYPE, false});
        if (!symbols_.declare(output)) report(output.name, output.line);
    }

    void declare_global(ASTNode* node) {
        Symbol symbol;
        switch (node->kind) {
            case NodeKind::VAR_DECL:
                symbol = make_symbol(node, SymbolKind::Variable);
                break;
            case NodeKind::ARRAY_DECL:
                symbol = make_symbol(node, SymbolKind::Array);
                break;
            case NodeKind::FUN_DECL: {
                symbol = make_symbol(node, SymbolKind::Function);
                for (ASTNode* parameter = child(node, 0);
                     parameter;
                     parameter = parameter->sibling) {
                    symbol.parameters.push_back({
                        parameter->type,
                        parameter->kind == NodeKind::PARAM_ARRAY
                    });
                }
                break;
            }
            default:
                return;
        }

        if (!symbols_.declare(symbol)) {
            report(symbol.name, node->line);
        }

        if ((node->kind == NodeKind::VAR_DECL ||
             node->kind == NodeKind::ARRAY_DECL) &&
            node->type == DataType::VOID_TYPE) {
            report(node->name, node->line);
        }
        if (node->kind == NodeKind::ARRAY_DECL && node->value <= 0) {
            report(node->name, node->line);
        }
    }

    void declare_globals(ASTNode* root) {
        for (ASTNode* declaration = root; declaration; declaration = declaration->sibling) {
            declare_global(declaration);
        }
    }

    void validate_main(ASTNode* root) {
        ASTNode* main_function = nullptr;
        ASTNode* last_declaration = nullptr;

        for (ASTNode* declaration = root; declaration; declaration = declaration->sibling) {
            last_declaration = declaration;
            if (declaration->kind == NodeKind::FUN_DECL &&
                declaration->name == "main") {
                main_function = declaration;
            }
        }

        if (!main_function) {
            report("main", root ? root->line : 1);
            return;
        }

        if (last_declaration != main_function) {
            report("main", main_function->line);
        }
        if (main_function->type != DataType::VOID_TYPE ||
            child(main_function, 0) != nullptr) {
            report("main", main_function->line);
        }
    }

    void analyze_function(ASTNode* function) {
        current_function_ = function->name;
        current_return_type_ = function->type;
        symbols_.enterScope(current_function_);

        for (ASTNode* parameter = child(function, 0);
             parameter;
             parameter = parameter->sibling) {
            Symbol symbol = make_symbol(
                parameter,
                parameter->kind == NodeKind::PARAM_ARRAY
                    ? SymbolKind::ArrayParameter
                    : SymbolKind::Parameter
            );
            if (!symbols_.declare(symbol)) {
                report(parameter->name, parameter->line);
            }
            if (parameter->type == DataType::VOID_TYPE) {
                report(parameter->name, parameter->line);
            }
        }

        ASTNode* body = child(function, 1);
        if (body && body->kind == NodeKind::COMPOUND_STMT) {
            analyze_compound_contents(body);
        }

        symbols_.exitScope();
    }

    void analyze_compound_contents(ASTNode* compound) {
        for (ASTNode* declaration = child(compound, 0);
             declaration;
             declaration = declaration->sibling) {
            declare_local(declaration);
        }

        for (ASTNode* statement = child(compound, 1);
             statement;
             statement = statement->sibling) {
            analyze_statement(statement);
        }
    }

    void declare_local(ASTNode* node) {
        Symbol symbol;
        if (node->kind == NodeKind::VAR_DECL) {
            symbol = make_symbol(node, SymbolKind::Variable);
        } else if (node->kind == NodeKind::ARRAY_DECL) {
            symbol = make_symbol(node, SymbolKind::Array);
        } else {
            return;
        }

        if (!symbols_.declare(symbol)) {
            report(node->name, node->line);
        }
        if (node->type == DataType::VOID_TYPE) {
            report(node->name, node->line);
        }
        if (node->kind == NodeKind::ARRAY_DECL && node->value <= 0) {
            report(node->name, node->line);
        }
    }

    void analyze_statement(ASTNode* statement) {
        if (!statement) return;

        switch (statement->kind) {
            case NodeKind::EXPR_STMT:
                analyze_expression(child(statement, 0));
                break;
            case NodeKind::COMPOUND_STMT:
                symbols_.enterScope(
                    current_function_ + "::block" + std::to_string(next_block_id_++)
                );
                analyze_compound_contents(statement);
                symbols_.exitScope();
                break;
            case NodeKind::IF_STMT:
                require_integer_scalar(
                    analyze_expression(child(statement, 0)),
                    child(statement, 0)
                );
                analyze_statement(child(statement, 1));
                analyze_statement(child(statement, 2));
                break;
            case NodeKind::WHILE_STMT:
                require_integer_scalar(
                    analyze_expression(child(statement, 0)),
                    child(statement, 0)
                );
                analyze_statement(child(statement, 1));
                break;
            case NodeKind::RETURN_STMT:
                analyze_return(statement);
                break;
            default:
                break;
        }
    }

    bool require_integer_scalar(const ExpressionInfo& info, const ASTNode* node) {
        if (!info.valid) return false;
        if (info.type != DataType::INT_TYPE || info.isArray) {
            report(identifier_for(node), node ? node->line : 0);
            return false;
        }
        return true;
    }

    void analyze_return(ASTNode* statement) {
        ASTNode* value = child(statement, 0);
        if (current_return_type_ == DataType::VOID_TYPE) {
            if (value) {
                analyze_expression(value);
                report(current_function_, statement->line);
            }
            return;
        }

        if (!value) {
            report(current_function_, statement->line);
            return;
        }

        require_integer_scalar(analyze_expression(value), value);
    }

    ExpressionInfo analyze_expression(ASTNode* expression) {
        if (!expression) return {DataType::INT_TYPE, false, false, false};

        switch (expression->kind) {
            case NodeKind::CONST_EXPR:
                return {DataType::INT_TYPE, false, false, true};
            case NodeKind::VAR_EXPR:
                return analyze_variable(expression);
            case NodeKind::ARRAY_EXPR:
                return analyze_array_access(expression);
            case NodeKind::CALL_EXPR:
                return analyze_call(expression);
            case NodeKind::OP_EXPR: {
                const ExpressionInfo left = analyze_expression(child(expression, 0));
                const ExpressionInfo right = analyze_expression(child(expression, 1));
                const bool left_valid = require_integer_scalar(
                    left, child(expression, 0) ? child(expression, 0) : expression
                );
                const bool right_valid = require_integer_scalar(
                    right, child(expression, 1) ? child(expression, 1) : expression
                );
                return {DataType::INT_TYPE, false, false, left_valid && right_valid};
            }
            case NodeKind::ASSIGN_EXPR: {
                ASTNode* left_node = child(expression, 0);
                ASTNode* right_node = child(expression, 1);
                const ExpressionInfo left = analyze_expression(left_node);
                const ExpressionInfo right = analyze_expression(right_node);
                bool valid = left.valid && right.valid;

                if (left.valid && (!left.isLValue || left.isArray)) {
                    report(identifier_for(left_node), left_node->line);
                    valid = false;
                }
                if (right.valid && (right.type != DataType::INT_TYPE || right.isArray)) {
                    report(identifier_for(right_node), right_node->line);
                    valid = false;
                }
                return {left.type, false, false, valid};
            }
            default:
                return {DataType::INT_TYPE, false, false, false};
        }
    }

    ExpressionInfo analyze_variable(ASTNode* expression) {
        const Symbol* symbol = symbols_.lookup(expression->name);
        if (!symbol) {
            report(expression->name, expression->line);
            return {DataType::INT_TYPE, false, false, false};
        }

        if (symbol->kind == SymbolKind::Variable ||
            symbol->kind == SymbolKind::Parameter) {
            return {symbol->type, false, true, true};
        }
        if (is_array_kind(symbol->kind)) {
            return {symbol->type, true, false, true};
        }

        report(expression->name, expression->line);
        return {symbol->type, false, false, false};
    }

    ExpressionInfo analyze_array_access(ASTNode* expression) {
        const Symbol* symbol = symbols_.lookup(expression->name);
        const ExpressionInfo index = analyze_expression(child(expression, 0));
        const bool index_valid = require_integer_scalar(index, child(expression, 0));

        if (!symbol) {
            report(expression->name, expression->line);
            return {DataType::INT_TYPE, false, true, false};
        }
        if (!is_array_kind(symbol->kind)) {
            report(expression->name, expression->line);
            return {symbol->type, false, true, false};
        }

        return {
            symbol->type,
            false,
            true,
            index_valid && symbol->type == DataType::INT_TYPE
        };
    }

    ExpressionInfo analyze_call(ASTNode* call) {
        std::vector<ExpressionInfo> arguments;
        std::vector<ASTNode*> argument_nodes;
        for (ASTNode* argument = child(call, 0);
             argument;
             argument = argument->sibling) {
            argument_nodes.push_back(argument);
            arguments.push_back(analyze_expression(argument));
        }

        const Symbol* function = symbols_.lookup(call->name);
        if (!function) {
            report(call->name, call->line);
            return {DataType::INT_TYPE, false, false, false};
        }
        if (function->kind != SymbolKind::Function) {
            report(call->name, call->line);
            return {function->type, false, false, false};
        }

        bool valid = true;
        if (arguments.size() != function->parameters.size()) {
            report(call->name, call->line);
            valid = false;
        }

        const size_t count = std::min(arguments.size(), function->parameters.size());
        for (size_t i = 0; i < count; ++i) {
            const ParameterInfo& expected = function->parameters[i];
            const ExpressionInfo& actual = arguments[i];
            if (!actual.valid) {
                valid = false;
                continue;
            }

            if (actual.type != expected.type ||
                actual.isArray != expected.isArray) {
                report(identifier_for(argument_nodes[i]), argument_nodes[i]->line);
                valid = false;
            }
        }

        return {function->type, false, false, valid};
    }
};

} // namespace

bool analyze_semantics(ASTNode* root, SymbolTable& symbols) {
    SemanticAnalyzer analyzer(symbols);
    return analyzer.analyze(root);
}
