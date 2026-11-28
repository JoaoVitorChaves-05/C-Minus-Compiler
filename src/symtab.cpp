#include "symtab.hpp"

#include <algorithm>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace {

const char* type_name(DataType type) {
    return type == DataType::INT_TYPE ? "int" : "void";
}

const char* kind_name(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::Variable:      return "variavel";
        case SymbolKind::Array:         return "vetor";
        case SymbolKind::Function:      return "funcao";
        case SymbolKind::Parameter:     return "parametro";
        case SymbolKind::ArrayParameter:return "parametro vetor";
    }
    return "desconhecido";
}

} // namespace

SymbolTable::SymbolTable() : current_(nullptr) {
    enterScope("global");
}

Scope* SymbolTable::enterScope(const std::string& name) {
    std::unique_ptr<Scope> scope(new Scope);
    scope->name = name;
    scope->parent = current_;

    Scope* entered_scope = scope.get();
    scopes_.push_back(std::move(scope));
    current_ = entered_scope;
    return current_;
}

bool SymbolTable::exitScope() {
    if (!current_ || !current_->parent) {
        return false;
    }

    current_ = current_->parent;
    return true;
}

bool SymbolTable::declare(const Symbol& symbol) {
    if (!current_ || symbol.name.empty()) {
        return false;
    }

    return current_->symbols.emplace(symbol.name, symbol).second;
}

const Symbol* SymbolTable::lookup(const std::string& name) const {
    for (const Scope* scope = current_; scope; scope = scope->parent) {
        const auto symbol = scope->symbols.find(name);
        if (symbol != scope->symbols.end()) {
            return &symbol->second;
        }
    }

    return nullptr;
}

const Symbol* SymbolTable::lookupCurrent(const std::string& name) const {
    if (!current_) {
        return nullptr;
    }

    const auto symbol = current_->symbols.find(name);
    return symbol == current_->symbols.end() ? nullptr : &symbol->second;
}

const Scope* SymbolTable::currentScope() const {
    return current_;
}

void SymbolTable::print(std::ostream& output) const {
    for (const std::unique_ptr<Scope>& scope : scopes_) {
        output << "Escopo: " << scope->name << '\n';

        std::vector<std::string> names;
        names.reserve(scope->symbols.size());
        for (const auto& entry : scope->symbols) {
            names.push_back(entry.first);
        }
        std::sort(names.begin(), names.end());

        for (const std::string& name : names) {
            const Symbol& symbol = scope->symbols.at(name);
            output << "  " << symbol.name << " : " << kind_name(symbol.kind)
                   << ", tipo " << type_name(symbol.type);
            if (symbol.kind == SymbolKind::Array) {
                output << ", tamanho " << symbol.arraySize;
            }
            if (symbol.kind == SymbolKind::Function) {
                output << ", parametros (";
                for (size_t i = 0; i < symbol.parameters.size(); ++i) {
                    if (i > 0) output << ", ";
                    output << type_name(symbol.parameters[i].type);
                    if (symbol.parameters[i].isArray) output << "[]";
                }
                output << ')';
            }
            if (symbol.line > 0) {
                output << ", linha " << symbol.line;
            }
            output << '\n';
        }
    }
}
