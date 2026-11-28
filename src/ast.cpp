#include "ast.hpp"
#include <algorithm>
#include <iostream>

static std::vector<bool> tree_ancestor_is_last;

ASTNode* new_node(NodeKind kind, int line) {
    return new ASTNode(kind, line);
}

ASTNode* append_sibling(ASTNode* list, ASTNode* node) {
    if (!list) return node;
    if (!node) return list;
    ASTNode* curr = list;
    while (curr->sibling != nullptr) {
        curr = curr->sibling;
    }
    curr->sibling = node;
    return list;
}

void free_ast(ASTNode* root) {
    if (!root) return;
    for (ASTNode* child : root->children) {
        free_ast(child);
    }
    free_ast(root->sibling);
    delete root;
}

static const char* type_to_str(DataType t) {
    return (t == DataType::INT_TYPE) ? "int" : "void";
}

int getHeight(ASTNode* root) {
    int height = 0;
    for (ASTNode* node = root; node; node = node->sibling) {
        int child_height = 0;
        for (ASTNode* child : node->children) {
            child_height = std::max(child_height, getHeight(child));
        }
        height = std::max(height, child_height + 1);
    }
    return height;
}

void fillBuffer(ASTNode* node, int level, int has_parent, int is_last) {
    if (!node) return;

    for (int ancestor = 0; ancestor < level - 1; ++ancestor) {
        std::cout << (tree_ancestor_is_last[ancestor] ? "    " : "|   ");
    }

    if (has_parent) {
        std::cout << (is_last ? "`-- " : "|-- ");
    }

    const std::string line = " (linha " + std::to_string(node->line) + ")";
    switch (node->kind) {
        case NodeKind::VAR_DECL:
            std::cout << "VarDecl: " << type_to_str(node->type) << " " << node->name << line;
            break;
        case NodeKind::ARRAY_DECL:
            std::cout << "ArrayDecl: " << type_to_str(node->type) << " " << node->name
                      << "[" << node->value << "]" << line;
            break;
        case NodeKind::FUN_DECL:
            std::cout << "FunDecl: " << type_to_str(node->type) << " " << node->name << line;
            break;
        case NodeKind::PARAM_VAR:
            std::cout << "Param: " << type_to_str(node->type) << " " << node->name;
            break;
        case NodeKind::PARAM_ARRAY:
            std::cout << "ParamArray: " << type_to_str(node->type) << " " << node->name << "[]";
            break;
        case NodeKind::COMPOUND_STMT: std::cout << "CompoundStmt" << line; break;
        case NodeKind::IF_STMT:       std::cout << "IfStmt" << line; break;
        case NodeKind::WHILE_STMT:    std::cout << "WhileStmt" << line; break;
        case NodeKind::RETURN_STMT:   std::cout << "ReturnStmt" << line; break;
        case NodeKind::EXPR_STMT:     std::cout << "ExprStmt" << line; break;
        case NodeKind::ASSIGN_EXPR:   std::cout << "Assign (=)" << line; break;
        case NodeKind::OP_EXPR:       std::cout << "Op: " << node->name << line; break;
        case NodeKind::VAR_EXPR:      std::cout << "Id: " << node->name << line; break;
        case NodeKind::ARRAY_EXPR:    std::cout << "ArrayAccess: " << node->name << line; break;
        case NodeKind::CALL_EXPR:     std::cout << "Call: " << node->name << line; break;
        case NodeKind::CONST_EXPR:    std::cout << "Const: " << node->value << line; break;
    }
    std::cout << '\n';

    std::vector<ASTNode*> children;
    for (ASTNode* child : node->children) {
        for (ASTNode* sibling = child; sibling; sibling = sibling->sibling) {
            children.push_back(sibling);
        }
    }

    if (level > 0) {
        tree_ancestor_is_last.resize(level);
        tree_ancestor_is_last[level - 1] = is_last != 0;
    }

    for (size_t i = 0; i < children.size(); ++i) {
        fillBuffer(children[i], level + 1, 1, i + 1 == children.size());
    }
}

void printTree(ASTNode* root) {
    if (getHeight(root) == 0) return;

    std::cout << "Program\n";
    tree_ancestor_is_last.clear();

    std::vector<ASTNode*> declarations;
    for (ASTNode* node = root; node; node = node->sibling) {
        declarations.push_back(node);
    }

    for (size_t i = 0; i < declarations.size(); ++i) {
        fillBuffer(declarations[i], 1, 1, i + 1 == declarations.size());
    }
}

void print_ast_text(ASTNode* node, int indent) {
    while (node != nullptr) {
        for (int i = 0; i < indent; ++i) std::cout << "  ";

        switch (node->kind) {
            case NodeKind::VAR_DECL:
                std::cout << "VarDecl: " << type_to_str(node->type) << " " << node->name << " (linha " << node->line << ")\n";
                break;
            case NodeKind::ARRAY_DECL:
                std::cout << "ArrayDecl: " << type_to_str(node->type) << " " << node->name << "[" << node->value << "] (linha " << node->line << ")\n";
                break;
            case NodeKind::FUN_DECL:
                std::cout << "FunDecl: " << type_to_str(node->type) << " " << node->name << " (linha " << node->line << ")\n";
                break;
            case NodeKind::PARAM_VAR:
                std::cout << "Param: " << type_to_str(node->type) << " " << node->name << "\n";
                break;
            case NodeKind::PARAM_ARRAY:
                std::cout << "ParamArray: " << type_to_str(node->type) << " " << node->name << "[]\n";
                break;
            case NodeKind::COMPOUND_STMT:
                std::cout << "CompoundStmt (linha " << node->line << ")\n";
                break;
            case NodeKind::IF_STMT:
                std::cout << "IfStmt (linha " << node->line << ")\n";
                break;
            case NodeKind::WHILE_STMT:
                std::cout << "WhileStmt (linha " << node->line << ")\n";
                break;
            case NodeKind::RETURN_STMT:
                std::cout << "ReturnStmt (linha " << node->line << ")\n";
                break;
            case NodeKind::EXPR_STMT:
                std::cout << "ExprStmt (linha " << node->line << ")\n";
                break;
            case NodeKind::ASSIGN_EXPR:
                std::cout << "Assign (=) (linha " << node->line << ")\n";
                break;
            case NodeKind::OP_EXPR:
                std::cout << "Op: " << node->name << " (linha " << node->line << ")\n";
                break;
            case NodeKind::VAR_EXPR:
                std::cout << "Id: " << node->name << " (linha " << node->line << ")\n";
                break;
            case NodeKind::ARRAY_EXPR:
                std::cout << "ArrayAccess: " << node->name << " (linha " << node->line << ")\n";
                break;
            case NodeKind::CALL_EXPR:
                std::cout << "Call: " << node->name << " (linha " << node->line << ")\n";
                break;
            case NodeKind::CONST_EXPR:
                std::cout << "Const: " << node->value << " (linha " << node->line << ")\n";
                break;
        }

        for (ASTNode* child : node->children) {
            print_ast_text(child, indent + 1);
        }

        node = node->sibling;
    }
}

static std::string node_label(ASTNode* node) {
    switch (node->kind) {
        case NodeKind::VAR_DECL:      return std::string("VarDecl\\n") + type_to_str(node->type) + " " + node->name;
        case NodeKind::ARRAY_DECL:    return std::string("ArrayDecl\\n") + type_to_str(node->type) + " " + node->name + "[" + std::to_string(node->value) + "]";
        case NodeKind::FUN_DECL:      return std::string("FunDecl\\n") + type_to_str(node->type) + " " + node->name;
        case NodeKind::PARAM_VAR:     return std::string("Param\\n") + type_to_str(node->type) + " " + node->name;
        case NodeKind::PARAM_ARRAY:   return std::string("ParamArray\\n") + type_to_str(node->type) + " " + node->name + "[]";
        case NodeKind::COMPOUND_STMT: return "CompoundStmt";
        case NodeKind::IF_STMT:       return "IfStmt";
        case NodeKind::WHILE_STMT:    return "WhileStmt";
        case NodeKind::RETURN_STMT:   return "ReturnStmt";
        case NodeKind::EXPR_STMT:     return "ExprStmt";
        case NodeKind::ASSIGN_EXPR:   return "Assign (=)";
        case NodeKind::OP_EXPR:       return "Op (" + node->name + ")";
        case NodeKind::VAR_EXPR:      return "Id: " + node->name;
        case NodeKind::ARRAY_EXPR:    return "Array: " + node->name;
        case NodeKind::CALL_EXPR:     return "Call: " + node->name;
        case NodeKind::CONST_EXPR:    return "Const: " + std::to_string(node->value);
    }
    return "Node";
}

static int write_dot_rec(ASTNode* node, std::ofstream& out, int& counter) {
    if (!node) return -1;
    int my_id = counter++;
    out << "  node" << my_id << " [label=\"" << node_label(node) << "\"];\n";

    for (ASTNode* child : node->children) {
        if (child) {
            int child_id = write_dot_rec(child, out, counter);
            out << "  node" << my_id << " -> node" << child_id << ";\n";
        }
    }

    if (node->sibling) {
        int sib_id = write_dot_rec(node->sibling, out, counter);
        out << "  node" << my_id << " -> node" << sib_id << " [style=dashed, label=\"next\"];\n";
    }
    return my_id;
}

void export_ast_dot(ASTNode* root, const std::string& filename) {
    std::ofstream out(filename);
    if (!out.is_open()) return;
    out << "digraph AST {\n";
    out << "  node [shape=box, fontname=\"Arial\"];\n";
    int counter = 0;
    write_dot_rec(root, out, counter);
    out << "}\n";
    out.close();
}