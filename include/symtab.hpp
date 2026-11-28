#ifndef SYMTAB_HPP
#define SYMTAB_HPP

#include "ast.hpp"

#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

enum class SymbolKind {
    Variable,
    Array,
    Function,
    Parameter,
    ArrayParameter
};

struct ParameterInfo {
    DataType type = DataType::INT_TYPE;
    bool isArray = false;
};

struct Symbol {
    std::string name;
    SymbolKind kind = SymbolKind::Variable;
    DataType type = DataType::INT_TYPE;
    int line = 0;
    int arraySize = 0;
    std::vector<ParameterInfo> parameters;
};

struct Scope {
    std::string name;
    Scope* parent = nullptr;
    std::unordered_map<std::string, Symbol> symbols;
};

class SymbolTable {
public:
    SymbolTable();

    Scope* enterScope(const std::string& name);
    bool exitScope();

    bool declare(const Symbol& symbol);
    const Symbol* lookup(const std::string& name) const;
    const Symbol* lookupCurrent(const std::string& name) const;

    const Scope* currentScope() const;
    void print(std::ostream& output) const;

private:
    std::vector<std::unique_ptr<Scope>> scopes_;
    Scope* current_;
};

#endif // SYMTAB_HPP