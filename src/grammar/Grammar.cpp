#include "Grammar.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/common.h"
#include "src/grammar/Grammar.h"
#include "src/grammar/GrammarReader.h"
#include "src/util/Formatter.h"
#include "src/display/steps.h"

using std::unordered_map;
using std::unordered_set;
using std::vector;

namespace gram {

Grammar::Grammar() {
    // Add built-in symbols
    epsilon = putSymbol(Constants::epsilon, true);
    addAlias(epsilon, "_e");
    addAlias(epsilon, "\\e");
    addAlias(epsilon, "\\epsilon");
    endOfInput = putSymbol(Constants::end_of_input, true);
}

auto Grammar::putSymbolNoDuplicate(Symbol &&sym) -> SymbolID {
    // `id` in sym is just the next id, we should check name instead
    auto it = idTable.find(sym.name);
    // Found
    if (it != idTable.end()) {
        auto &storedSym = symbols[it->second];
        if (storedSym.type == SymbolType::UNCHECKED) {
            storedSym.type = sym.type;
            return storedSym.id;
        }
        // This item attempts to overwrite symbol type
        if (sym.type != SymbolType::UNCHECKED) {
            if (sym.type == SymbolType::NON_TERM) {
                // A symbol can be upgraded to a non-terminal.
                storedSym.type = SymbolType::NON_TERM;
            }
        }
        return storedSym.id;
    }
    // Not found
    // Do not rely on `id` in arg `sym`
    auto symid = static_cast<SymbolID>(symbols.size());
    idTable.emplace(sym.name, symid);
    // Use `move` after sym.name is copyed to idTable
    symbols.push_back(std::move(sym));
    return symid;
}

auto Grammar::putSymbol(const char *name, bool isTerm) -> SymbolID {
    Symbol sym{isTerm ? SymbolType::TERM : SymbolType::NON_TERM,
               static_cast<SymbolID>(symbols.size()), name};
    return putSymbolNoDuplicate(std::move(sym));
}

// Delays checking
auto Grammar::putSymbolUnchecked(const char *name) -> SymbolID {
    Symbol sym{SymbolType::UNCHECKED,
               static_cast<SymbolID>(symbols.size()), name};
    return putSymbolNoDuplicate(std::move(sym));
}

void Grammar::addAlias(SymbolID id, const char *alias) {
    if (symbols.size() <= static_cast<size_t>(id)) {
        throw std::runtime_error("No such symbol: " + std::to_string(id));
    }
    idTable.emplace(alias, id);
}

ProductionID Grammar::addProduction(SymbolID leftSymbol,
                                    std::vector<SymbolID> rightSymbols) {
    // Production ID
    auto id = (ProductionID)productionTable.size();
    productionTable.emplace_back(leftSymbol, std::move(rightSymbols));
    symbols[leftSymbol].productions.push_back(id);
    return id;
}

ProductionTable const &Grammar::getProductionTable() const {
    return productionTable;
}

std::string Grammar::dump() const {
    std::string s;
    util::Formatter f;

    s += "Symbols:\n";
    auto const &vec = symbols;
    for (size_t i = 0; i < vec.size(); ++i) {
        s += f.formatView(
            "    %zd) %s %s", i, vec[i].name.c_str(),
            vec[i].type == SymbolType::TERM ? "[TERM" : "[NONTERM");
        if (vec[i].id == getStartSymbol().id) {
            s += ",START";
        }
        s += "]\n";
        step::symbol(i, vec[i].name.c_str(), vec[i].type == SymbolType::TERM,
                   vec[i].id == getStartSymbol().id);
    }

    s += "Productions:";
    // int productionCounter = 0;
    auto const &table = productionTable;
    auto table_size = table.size();
    // for (auto &production : productionTable) {
    for (size_t i = 0; i < table_size; ++i) {
        s += "\n    ";
        s += std::to_string(i);
        s += ") ";
        s += vec[table[i].leftSymbol].name;
        s += " ->";
        for (auto sym : table[i].rightSymbols) {
            s += ' ';
            s += vec[sym].name;
        }
        if (table[i].rightSymbols.empty()) {
            s += ' ';
            s += Constants::epsilon;
        }
        step::production(i, table[i].leftSymbol,
                       (const int *)table[i].rightSymbols.data(),
                       table[i].rightSymbols.size());
    }

    return s;
}

auto Grammar::fromFile(const char *fileName) -> Grammar {
    std::fstream stream(fileName, std::ios::in);
    if (!stream.is_open()) {
        throw std::runtime_error(std::string("File not found: ") + fileName);
    }
    auto g = GrammarReader::parse(stream);
    display(GRAMMAR_RULES, INFO, "Grammar rules has been parsed", &g);
    g.calAttributes();
    return g;
}

auto Grammar::fromStdin() -> Grammar {
    return GrammarReader::parse(::std::cin).calAttributes();
}

void Grammar::checkViolations() {
    // Check if there's a sym with no type
    for (auto &sym : symbols) {
        if (sym.type == SymbolType::UNCHECKED) {
            throw UnsolvedSymbolError(sym);
        }
    }
    // TODO: check if there's a A -> A
}

// setStart() should only be called once.
void Grammar::setStart(const char *name) {
    // Although we know start symbol must not be a terminal,
    // we cannot define it here, we need to check symbol later.
    start = putSymbolUnchecked(name);

    // Since this is the start, there are no productions yet.
}

void Grammar::calNullable() {
    for (auto &sym : symbols) {
        sym.nullable = false;
        // Only display terminal attributes to GUI.
        if (sym.type == SymbolType::TERM) {
            step::nullable(sym.id, false, nullptr); // No explanation.
        }
    }
    symbols[epsilon].nullable = true;
    step::nullable(epsilon, true, nullptr); // No explanation.
    // Explain here.
    step::show("Epsilon is nullable, while other terminals are not.");

    bool change = true;
    while (change) {
        change = false;
        for (auto &prod : productionTable) {
            bool nullableFlag = true;
            for (auto rhs : prod.rightSymbols) {
                nullableFlag = nullableFlag && symbols[rhs].nullable;
            }
            auto &left = symbols[prod.leftSymbol];
            if (nullableFlag && !left.nullable) {
                change = true;
                left.nullable = nullableFlag;
                // Build message.
                std::string msg;
                msg.reserve(128);
                msg += "Production ";
                msg += dumpProduction(prod);
                msg += " can be null, so ";
                msg += left.name;
                msg += " is nullable.";
                step::nullable(prod.leftSymbol, true, msg.c_str());
            }
        }
    }

    // Synchronize nullable attributes with GUI.
    for (auto &sym : symbols) {
        step::nullable(sym.id, sym.nullable, nullptr); // No explanation.
    }
    step::show("All other symbols cannot be null.");
}

void Grammar::calFirst() {
    for (auto &sym : symbols) {
        if (sym.type == SymbolType::TERM) {
            sym.firstSet.insert(sym.id);
            step::addFirst(sym.id, sym.id, nullptr); // No explanation.
        }
    }
    // Explain here.
    step::show("First set of each terminal only contains itself.");

    bool change = true;
    while (change) {
        change = false;
        for (auto &left : symbols) {
            if (left.type == SymbolType::TERM)
                continue;
            auto newFirst = Symbol::SymbolSet();
            for (auto &pindex : left.productions) {
                auto const &rhs = productionTable[pindex].rightSymbols;
                for (auto right : rhs) {
                    newFirst |= symbols[right].firstSet;
                    step::mergeFirst(left.id, right, nullptr); // No expl.
                    if (!symbols[right].nullable)
                        break;
                }
                if (!left.firstSet.supersetOf(newFirst)) {
                    change = true;
                    left.firstSet |= newFirst;
                    std::string msg =
                        "Rule: If X → Y₁Y₂…YₐYₑ…, Y₁Y₂…Yₐ are all "
                        "nullable, <br/>but Yₑ is not nullable, it follows that First(a) ⊆ First(X) "
                        "for a ∈ {Y₁, Y₂, …, Yₐ}.<br/>";
                    msg += "First set of symbol ";
                    msg += left.name;
                    msg += " is updated by production ";
                    msg += dumpProductionHtml(pindex, (int)rhs.size());
                    msg += ".";
                    step::show(msg);
                }
            }
        }
    }
}

void Grammar::calFollow() {
    symbols[start].followSet.insert(endOfInput);
    step::addFollow(start, endOfInput, "Follow set of start symbol contains $");

    for (auto const &production : productionTable) {
        auto lhs = production.leftSymbol;
        auto const &rhs = production.rightSymbols;
        auto len = (int)rhs.size();
        for (int i = 0; i + 1 < len; ++i) { // i and i+1 both have elements
            if (symbols[rhs[i]].type == SymbolType::NON_TERM) {
                symbols[rhs[i]].followSet |= symbols[rhs[i+1]].firstSet;
                step::mergeFollowFromFirst(rhs[i], rhs[i+1], epsilon, nullptr);
            }
        }
        std::string msg =
            "Rule: If A → aBb ∈ P, First(b) - {ε} ⊆ Follow(B).<br/>";
        msg += "Follow set of symbol ";
        msg += symbols[lhs].name;
        msg += " is updated by production ";
        msg += dumpProductionHtml(production, len);
        msg += ".";
        step::show(msg);
    }

    std::string rule = "Rule: If A → aBb ∈ P and b is nullable, or A → aB, "
                       "then Follow(A) ⊆ Follow(B).<br/>";

    bool change = true;
    while (change) {
        change = false;
        for (auto const &production : productionTable) {
            auto lhs = production.leftSymbol;
            auto const &rhs = production.rightSymbols;
            auto len = (int)rhs.size();
            for (int i = len - 1; i >= 0; --i) {
                if (symbols[rhs[i]].type == SymbolType::NON_TERM) {
                    auto newFollow = symbols[rhs[i]].followSet;
                    newFollow |= symbols[lhs].followSet;
                    if (newFollow != symbols[rhs[i]].followSet) { // Update
                        symbols[rhs[i]].followSet = newFollow;
                        change = true;
                        std::string msg = rule;
                        msg += "Follow set of symbol ";
                        msg += symbols[rhs[i]].name;
                        msg += " is updated by production ";
                        msg += dumpProductionHtml(production, i);
                        msg += ".";
                        step::mergeFollow(rhs[i], lhs, msg.c_str());
                    }
                }
                if (!symbols[rhs[i]].nullable)
                    break;
            }
        }
    }

    // For completion, remove epsilons from Follow sets.
    // The removal has no effect on calculation, though.
    for (auto &sym : symbols) {
        sym.followSet.remove(epsilon);
    }
}

Grammar &Grammar::calAttributes() {
    // classifySymbols();

    // stepPrintf("# Grammar file is read.\n");
    step::section("Attributes");

    calNullable();
    calFirst();
    calFollow();

    // // Nullable
    // // Epsilon is nullable // This is done in resolveNullable
    // // symbolVector[epsilon].nullable.emplace(true);

    // // Apply 2 more rules:
    // // For t in T, t is not nullable
    // // For A -> a...b, A is nullable <=> a ... b are all nullable
    // for (auto &symbol : symbolVector) {
    //     resolveNullable(symbol);
    // }


    // // First Set

    // vector<int> visit(symbolVector.size(), 0);
    // // For t in T, First(t) = {t}
    // for (auto &symbol : symbolVector) {
    //     if (symbol.type == SymbolType::TERM) {
    //         symbol.firstSet.insert(symbol.id);
    //         visit[symbol.id] = 1;
    //         step::addFirst(symbol.id, symbol.id,
    //                      "The first set of a terminal contains itself.");
    //     }
    // }

    // // For a in T or N, if a -*-> epsilon, then epsilon is in First(a)
    // for (auto &symbol : symbolVector) {
    //     if (symbol.nullable.value()) {
    //         symbol.firstSet.insert(epsilon);
    //         step::addFirst(
    //             symbol.id, epsilon,
    //             "The first set of any nullable symbol contains epsilon.");
    //     }
    // }

    // // For n in T, check production chain
    // for (auto &symbol : symbolVector) {
    //     resolveFirstSet(visit, symbol);
    // }

    // // Ambiguous grammar may have dependency circles. Resolve attributes again.
    // // I'm not sure if resolving first sets twice can fully solve the problem...
    // std::fill(visit.begin(), visit.end(), 0);
    // for (int N : this->nonterminals) {
    //     resolveFirstSet(visit, symbolVector[N]);
    // }

    // // display(SYMBOL_TABLE, INFO, "Calculate first set", this);

    // // Follow Set

    // // Add $ to Follow set of the start symbol
    // symbolVector[start].followSet.insert(endOfInput);
    // step::addFollow(start, endOfInput,
    //               "The follow set of start symbol contains end-of-input.");

    // // Get symbols from the next symbol's First set, and generate
    // // a dependency graph.
    // // e.g. table[a] = {b, c} ===> a needs b and c
    // unordered_map<SymbolID, unordered_set<SymbolID>> dependencyTable;

    // for (auto const &symbol : symbolVector) {
    //     // If this for-loop is entered, the symbol cannot be a
    //     // terminal.
    //     for (auto prodID : symbol.productions) {
    //         auto const &prod = productionTable[prodID];
    //         auto const &productionBody = prod.rightSymbols;
    //         // Skip epsilon productions
    //         if (productionBody.empty())
    //             continue;

    //         auto const &last = symbolVector[productionBody.back()];

    //         // Only calculate Follow sets for non-terminals
    //         if (last.type == SymbolType::NON_TERM)
    //             dependencyTable[last.id].insert(prod.leftSymbol);

    //         long size = static_cast<long>(productionBody.size());
    //         for (long i = size - 2; i >= 0; --i) {
    //             // Only calculate Follow sets for non-terminals
    //             auto &thisSymbol = symbolVector[productionBody[i]];
    //             auto const &nextSymbol = symbolVector[productionBody[i + 1]];

    //             if (thisSymbol.type != SymbolType::NON_TERM) {
    //                 continue;
    //             }

    //             for (auto first : nextSymbol.firstSet) {
    //                 if (first != epsilon) {
    //                     thisSymbol.followSet.insert(first);
    //                     step::addFollow(
    //                         thisSymbol.id, first,
    //                         "For A -> …ab…, all non-epsilon symbols "
    //                         "in b's first set appear in a's follow set.");
    //                 }
    //             }
    //             if (nextSymbol.nullable.value())
    //                 dependencyTable[thisSymbol.id].insert(nextSymbol.id);
    //         }
    //     }
    // }

    // // Figure out dependencies
    // std::fill(visit.begin(), visit.end(), 0);
    // for (auto &entry : dependencyTable) {
    //     resolveFollowSet(visit, dependencyTable, entry);
    // }

    display(SYMBOL_TABLE, INFO, "Calculate symbol attributes", this);

    // std::fflush(stdout); // For debug

    return *this;
}

static std::string dumpSymbolSet(Grammar const &g,
                                 Symbol::SymbolSet const &symset) {
    std::string s = "{";
    auto const &symvec = g.getAllSymbols();
    for (auto symid : symset) {
        s += ' ';
        s += symvec[symid].name;
    }
    s += (s.size() == 1) ? "}" : " }";
    return s;
}

std::string Grammar::dumpNullable(const Symbol &symbol) {
    return symbol.nullable ? "true" : "false";
}

std::string Grammar::dumpFirstSet(const Symbol &symbol) const {
    return dumpSymbolSet(*this, symbol.firstSet);
}

std::string Grammar::dumpFollowSet(const Symbol &symbol) const {
    return dumpSymbolSet(*this, symbol.followSet);
}

std::string Grammar::dumpProduction(ProductionID prodID) const {
    return dumpProduction(productionTable[prodID]);
}

std::string Grammar::dumpProductionHtml(ProductionID prodID, int underline) const {
    return dumpProductionHtml(productionTable[prodID], underline);
}

std::string Grammar::dumpProduction(const Production &production) const {
    std::string s;
    s += symbols[production.leftSymbol].name;
    s += " →";
    for (int rightSymbol : production.rightSymbols) {
        s += ' ';
        s += symbols[rightSymbol].name;
    }
    return s;
}

std::string Grammar::dumpProductionHtml(const Production &production,
                                        int underline) const {
    std::string s;
    auto len = (int)production.rightSymbols.size();
    if (underline == len) s += "<u>";
    s += symbols[production.leftSymbol].name;
    if (underline == len) s += "</u>";
    s += " →";
    for (int i = 0; i < len; ++i) {
        int cur = production.rightSymbols[i];
        s += ' ';
        if (underline == i) s += "<u>";
        s += symbols[cur].name;
        if (underline == i) s += "</u>";
    }
    return s;
}

const Symbol &Grammar::getEndOfInputSymbol() const {
    return symbols[endOfInput];
}

const Symbol &Grammar::getEpsilonSymbol() const {
    return symbols[epsilon];
}

const Symbol &Grammar::getStartSymbol() const { return symbols[start]; }

const Grammar::symvec_t &Grammar::getAllSymbols() const { return symbols; }

Symbol const &Grammar::findSymbol(std::string const &s) const {
    auto it = idTable.find(s);
    if (it != idTable.end())
        return symbols[it->second];
    throw NoSuchSymbolError(s);
}

} // namespace gram
