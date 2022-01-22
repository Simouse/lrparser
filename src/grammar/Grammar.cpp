#include "Grammar.h"

#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/common.h"
#include "src/grammar/GrammarReader.h"

using std::unordered_map;
using std::unordered_set;
using std::vector;

namespace gram {

Grammar::Grammar() {
    // Add built-in symbols
    epsilon = putSymbol(Grammar::SignStrings::epsilon, true);
    addAlias(epsilon, "_e");
    addAlias(epsilon, "\\e");
    addAlias(epsilon, "\\epsilon");
    endOfInput = putSymbol(Grammar::SignStrings::endOfInput, true);
}

auto Grammar::putSymbolNoDuplicate(Symbol &&sym) -> int {
    // `id` in sym is just the next id, we should check name instead
    auto it = idTable.find(sym.name);
    // Found
    if (it != idTable.end()) {
        auto &storedSym = symVec[it->second];
        if (storedSym.type == SymbolType::UNCHECKED) {
            storedSym.type = sym.type;
            return storedSym.id;
        }
        if (storedSym.type != sym.type && sym.type != SymbolType::UNCHECKED) {
            throw std::runtime_error(
                "Redefinition of previous symbol with different types");
        }
        return storedSym.id;
    }
    // Not found
    // Do not rely on `id` in arg `sym`
    int symid = (int)symVec.size();
    idTable.emplace(sym.name, symid);
    // Use `move` after sym.name is copyed to idTable
    symVec.push_back(std::move(sym));
    return symid;
}

auto Grammar::putSymbol(const char *name, bool isTerm) -> int {
    Symbol sym{isTerm ? SymbolType::TERM : SymbolType::NON_TERM,
               (int)symVec.size(), name};
    return putSymbolNoDuplicate(std::move(sym));
}

// Delays checking
auto Grammar::putSymbolUnchecked(const char *name) -> int {
    Symbol sym{SymbolType::UNCHECKED, (int)symVec.size(), name};
    return putSymbolNoDuplicate(std::move(sym));
}

void Grammar::addAlias(int id, const char *alias) {
    if (symVec.size() <= static_cast<size_t>(id)) {
        throw std::runtime_error("No such symbol: " + std::to_string(id));
    }
    idTable.emplace(alias, id);
}

ProductionID Grammar::addProduction(int leftSymbol,
                                    std::vector<int> rightSymbols) {
    // Production ID
    auto id = (ProductionID)productionTable.size();
    productionTable.emplace_back(leftSymbol, std::move(rightSymbols));
    symVec[leftSymbol].productions.push_back(id);
    return id;
}

ProductionTable const &Grammar::getProductionTable() const {
    return productionTable;
}

String Grammar::dump() const {
    String s;
    const char *typeStr[] = {"    Nonterminals:\n", "    Terminals:\n"};
    vector<int> classifiedSyms[2];

    for (auto &sym : symVec) {
        classifiedSyms[(int)sym.type].push_back(sym.id);
    }
    s += "Symbols:\n";
    for (int i = 1; i >= 0; --i) {
        s += typeStr[i];
        for (int symid : classifiedSyms[i]) {
            auto &sym = symVec[symid];
            s += "        \"" + sym.name + "\", ";
            s += "id: " + std::to_string(sym.id);
            if (sym.id == getStartSymbol().id) {
                s += " [START]";
            }
            s += '\n';
        }
    }

    s += "Productions:";
    int productionCounter = 0;
    for (auto &production : productionTable) {
        s += "\n    ";
        s += std::to_string(productionCounter++);
        s += ") ";
        s += symVec[production.leftSymbol].name;
        s += " ->";
        for (auto sym : production.rightSymbols) {
            s += ' ';
            s += symVec[sym].name;
        }
    }

    return s;
}

auto Grammar::fromFile(const char *fileName) -> Grammar {
    std::fstream stream(fileName, std::ios::in);
    if (!stream.is_open()) {
        throw std::runtime_error(String("File not found: ") + fileName);
    }
    auto g = GrammarReader::parse(stream);
    display(DisplayType::GRAMMAR_RULES, &g, "Grammar rules has been parsed");
    g.fillSymbolAttrs();
    return g;
}

auto Grammar::fromStdin() -> Grammar {
    return GrammarReader::parse(::std::cin).fillSymbolAttrs();
}

void Grammar::checkViolations() {
    // Check if there's a sym with no type
    for (auto &sym : symVec) {
        if (sym.type == SymbolType::UNCHECKED) {
            throw UnsolvedSymbolError(sym);
        }
    }
    // TODO: check if there's a A -> A
}

void Grammar::setStart(const char *name) {
    // Although we know start symbol must not be a terminal,
    // we cannot define it here, we need to check symbol later.
    start = putSymbolUnchecked(name);
}

// This function needs to get a Symbol & from symVec,
// so symVec must be a mutable reference
bool Grammar::resolveNullable(Symbol &sym) {
    if (sym.nullable.has_value()) {
        return sym.nullable.value();
    }

    // Epsilon is nullable
    if (sym.id == epsilon) {
        sym.nullable.emplace(true);
        return true;
    }

    // Place false first to prevent infinite recursive calls
    sym.nullable.emplace(false);

    // For t in T, t is not nullable
    if (sym.type == SymbolType::TERM) {
        return false;
    }

    // For A -> a...b, A is nullable <=> a ... b are all nullable
    for (auto const &prodID : symVec[sym.id].productions) {
        auto const &production = productionTable[prodID];
        // This symbol has epsilon production
        if (production.rightSymbols.empty()) {
            sym.nullable.emplace(true);
            return true;
        }
        bool prodNullable = true;
        for (int rid : production.rightSymbols) {
            if (!resolveNullable(symVec[rid])) {
                prodNullable = false;
                break;
            }
        }
        if (prodNullable) {
            sym.nullable.emplace(true);
            return true;
        }
        // No luck, process the next production
    }
    return false;
}

// This function needs to get a Symbol & from symVec,
// so symVec must be a mutable reference
void Grammar::resolveFirstSet(vector<int> &visit, Symbol &curSymbol) {
    if (visit[curSymbol.id])
        return;

    // Mark the flag to prevent circular recursive call
    visit[curSymbol.id] = 1;

    for (auto const &prodID : symVec[curSymbol.id].productions) {
        auto const &production = productionTable[prodID];
        // This flag is true when the body of this production is nullable
        bool allNullable = true;
        for (int rid : production.rightSymbols) {
            auto &rightSymbol = symVec[rid];
            if (!visit[rid]) {
                resolveFirstSet(visit, rightSymbol);
            }
            for (int sid : rightSymbol.firstSet) {
                if (sid != epsilon)
                    curSymbol.firstSet.insert(sid);
            }
            if (!rightSymbol.nullable.value()) {
                allNullable = false;
                break;
            }
        }
        if (allNullable) {
            curSymbol.firstSet.insert(epsilon);
        }
    }
}

// This function figures out the dependencies among Follow sets.
void Grammar::resolveFollowSet(
    vector<int> &visit, unordered_map<int, unordered_set<int>> &dependencyTable,
    std::pair<const int, std::unordered_set<int>> &dependency) {

    if (visit[dependency.first]) {
        return;
    }
    visit[dependency.first] = 1;

    auto &parentSet = dependency.second;
    for (int parent : parentSet) {
        auto it = dependencyTable.find(parent);
        if (it != dependencyTable.end() && !it->second.empty()) {
            resolveFollowSet(visit, dependencyTable, *it);
        }
        // Add follow set items from parent
        auto const &parentFollowSet = symVec[parent].followSet;
        auto &followSet = symVec[dependency.first].followSet;
        followSet.insert(parentFollowSet.begin(), parentFollowSet.end());
    }
    parentSet.clear();
};

Grammar &Grammar::fillSymbolAttrs() {
    //--------------- Nullable ---------------
    // Epsilon is nullable
    symVec[epsilon].nullable.emplace(true);

    // Apply 2 more rules:
    // For t in T, t is not nullable
    // For A -> a...b, A is nullable <=> a ... b are all nullable
    for (auto &symbol : symVec) {
        resolveNullable(symbol);
    }

    display(DisplayType::SYMBOL_TABLE, this, "Calculate nullables");

    //--------------- First Set ---------------
    vector<int> visit(symVec.size(), 0);
    // For t in T, First(t) = {t}
    for (auto &symbol : symVec) {
        if (symbol.type == SymbolType::TERM) {
            symbol.firstSet.insert(symbol.id);
            visit[symbol.id] = 1;
        }
    }

    // For a in T or N, if a -*-> epsilon, then epsilon is in First(a)
    for (auto &symbol : symVec) {
        if (symbol.nullable.value()) {
            symbol.firstSet.insert(epsilon);
        }
    }

    // For n in T, check production chain
    for (auto &symbol : symVec) {
        resolveFirstSet(visit, symbol);
    }

    display(DisplayType::SYMBOL_TABLE, this, "Calculate first set");

    //--------------- Follow Set ---------------
    // Add $ to Follow set of the start symbol
    symVec[start].followSet.insert(endOfInput);

    // Get symbols from the next symbol's First set, and generate
    // a dependency graph.
    // e.g. table[a] = {b, c} ===> a needs b and c
    unordered_map<int, unordered_set<int>> dependencyTable;

    for (auto const &symbol : symVec) {
        // If this for-loop is entered, the symbol cannot be a
        // terminal.
        for (auto prodID : symbol.productions) {
            auto const &prod = productionTable[prodID];
            auto const &productionBody = prod.rightSymbols;
            // Skip epsilon productions
            if (productionBody.empty())
                continue;

            auto const &last = symVec[productionBody.back()];

            // Only calculate Follow sets for non-terminals
            if (last.type == SymbolType::NON_TERM)
                dependencyTable[last.id].insert(prod.leftSymbol);

            long size = static_cast<long>(productionBody.size());
            for (long i = size - 2; i >= 0; --i) {
                // Only calculate Follow sets for non-terminals
                auto &thisSymbol = symVec[productionBody[i]];
                auto const &nextSymbol = symVec[productionBody[i + 1]];

                if (thisSymbol.type != SymbolType::NON_TERM) {
                    continue;
                }

                for (int first : nextSymbol.firstSet) {
                    if (first != epsilon)
                        thisSymbol.followSet.insert(first);
                }
                if (nextSymbol.nullable.value())
                    dependencyTable[thisSymbol.id].insert(nextSymbol.id);
            }
        }
    }

    // Figure out dependencies
    std::fill(visit.begin(), visit.end(), 0);
    for (auto &entry : dependencyTable) {
        resolveFollowSet(visit, dependencyTable, entry);
    }

    display(DisplayType::SYMBOL_TABLE, this, "Calculate follow set");

    return *this;
}

static String dumpSymbolSet(Grammar const &g, Symbol::symset_t const &symset) {
    String s = "{";
    auto const &symvec = g.getAllSymbols();
    for (int symid : symset) {
        s += ' ';
        s += symvec[symid].name;
    }
    s += (s.size() == 1) ? "}" : " }";
    return s;
}

String Grammar::dumpNullable(const Symbol &symbol) {
    if (!symbol.nullable.has_value()) {
        return "?";
    }
    return symbol.nullable.value() ? "true" : "false";
}

String Grammar::dumpFirstSet(const Symbol &symbol) const {
    return dumpSymbolSet(*this, symbol.firstSet);
}

String Grammar::dumpFollowSet(const Symbol &symbol) const {
    return dumpSymbolSet(*this, symbol.followSet);
}

String Grammar::dumpProduction(ProductionID prodID) const {
    String s;
    auto const &production = productionTable[prodID];
    s += symVec[production.leftSymbol].name;
    s += " ->";
    for (int rightSymbol : production.rightSymbols) {
        s += ' ';
        s += symVec[rightSymbol].name;
    }
    return s;
}

const Symbol &Grammar::getEndOfInputSymbol() const {
    return symVec[endOfInput];
}

const Symbol &Grammar::getEpsilonSymbol() const { return symVec[epsilon]; }

const Symbol &Grammar::getStartSymbol() const { return symVec[start]; }

const Grammar::symvec_t &Grammar::getAllSymbols() const { return symVec; }

Symbol const &Grammar::findSymbol(String const &s) const {
    auto it = idTable.find(s);
    if (it != idTable.end())
        return symVec[it->second];
    throw NoSuchSymbolError(s);
}

} // namespace gram
