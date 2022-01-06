#include "../include/synctx.h"
#include "../include/synreader.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using std::string;
using std::vector;

namespace lr {
auto SyntaxContext::putSym(Symbol &&sym) -> int {
    // `id` in sym is just the next id, we should check name instead
    auto it = idTable.find(sym.name);
    // Found
    if (it != idTable.end()) { 
        auto &storedSym = symVec[it->second];
        if (storedSym.type == SymType::UNCHECKED) {
            storedSym.type = sym.type;
            return storedSym.id;
        }
        if (storedSym.type != sym.type && sym.type != SymType::UNCHECKED) {
            throw std::runtime_error(
                "Redefinition of previous symbol with different types");
        }
        return storedSym.id;
    }
    // Not found
    int symid = sym.id;
    idTable.emplace(sym.name, sym.id);
    symVec.push_back(std::move(sym)); // After emplace, because name should be copyed
    rules.emplace_back();
    return symid;
}

auto SyntaxContext::putSym(const char *name, bool isTerm) -> int {
    Symbol sym{isTerm ? SymType::TERM : SymType::NONTERM, (int)symVec.size(),
               name};
    return putSym(std::move(sym));
}

// Delays checking
auto SyntaxContext::putSym(const char *name) -> int {
    Symbol sym{SymType::UNCHECKED, (int)symVec.size(), name};
    return putSym(std::move(sym));
}

void SyntaxContext::addAlias(int id, const char *alias) {
    if (symVec.size() <= id) {
        throw std::runtime_error("No such symbol: " + std::to_string(id));
    }
    idTable.emplace(alias, id);
}

auto SyntaxContext::findSym(const char *s) const -> const Symbol & {
    auto result = idTable.find(s);
    if (result != idTable.end()) {
        return symVec[result->second];
    }
    std::string estr = "No such symbol: ";
    estr += s;
    throw std::runtime_error(estr);
}

auto SyntaxContext::findSym(int id) const -> const Symbol & {
    if (symVec.size() <= id) {
        throw std::runtime_error("No such symbol: " + std::to_string(id));
    }
    return symVec[id];
}

auto SyntaxContext::findSym(const char *s, bool isTerm) const
    -> const Symbol & {
    auto &sym = findSym(s);
    if ((int)sym.type != (int)isTerm) {
        throw std::runtime_error(
            "Symbol already defined but type was different");
    }
    return sym;
}

void SyntaxContext::putRules(int nid, vector<vector<int>> &&newRules) {
    rules[nid] = std::move(newRules);
}

auto SyntaxContext::fromFile(const char *fileName) -> SyntaxContext * {
    std::fstream stream(fileName, std::ios::in);
    if (!stream.is_open()) {
        throw std::runtime_error(string("File not found: ") + fileName);
    }
    SyntaxReader reader(stream);
    auto *ctx = new SyntaxContext();
    reader.parseRules(*ctx);
    return ctx;
}

auto SyntaxContext::fromStdin() -> SyntaxContext * {
    SyntaxReader reader(std::cin);
    auto *ctx = new SyntaxContext();
    reader.parseRules(*ctx);
    return ctx;
}

void SyntaxContext::checkSymTypes() {
    for (auto &sym: symVec) {
        if (sym.type == SymType::UNCHECKED) {
            throw UnsolvedSymbolError(sym);
        }
    }
}

auto SyntaxContext::getSymVec() const -> const decltype(symVec) & {
    return this->symVec;
}

auto SyntaxContext::getRules() const -> const decltype(rules) & {
    return this->rules;
}

auto SyntaxContext::getSym(int id) const -> const Symbol & {
    return this->symVec[id];
}
} // namespace lr