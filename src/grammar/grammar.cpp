#include "grammar.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <array>

#include "src/common.h"
#include "src/display/display.h"

using std::ifstream;
using std::isblank;
using std::isspace;
using std::istream;
using std::unordered_map;
using std::unordered_set;
using std::vector;

namespace gram {
// TODO: check if unnecessary terminals exist
class GrammarReader {
  private:
    int linenum = 0;
    // Make sure pos is empty but not NULL in the beginning
    const char *pos = "";
    // Only valid if pos != NULL and stream is read at least once
    const char *lineStart;
    String line, token;
    std::istream &stream;
    std::unordered_map<String, int> tokenLineNo;

    auto getLineAndCount(std::istream &is, String &s) -> bool;
    auto skipSpaces(const char *p) -> const char *;
    auto skipBlanks(const char *p) -> const char *;
    auto nextEquals(char ch) -> bool;
    auto expect(char ch) -> bool;
    void expectOrThrow(const char *expected);
    bool getToken(String &s, bool newlineAutoFetch = true);
    void ungetToken(const String &s);

    GrammarReader(std::istream &is) : stream(is) {}
    void parse(Grammar &g);

  public:
    static Grammar parse(std::istream &stream);
};
} // namespace gram

namespace gram {

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
    if (symVec.size() <= id) {
        throw std::runtime_error("No such symbol: " + std::to_string(id));
    }
    idTable.emplace(alias, id);
}

void Grammar::addRule(int nid, vector<vector<int>> &&newRule) {
    for (auto const &r : newRule) {
        addProduction(nid, r);
    }
    prodVecTable.emplace(nid, std::move(newRule));
}

ProductionID Grammar::addProduction(int leftSymbol, std::vector<int> rightSymbols) {
    auto id = (ProductionID)experimentProdTbl.size();
    experimentProdTbl.emplace_back(leftSymbol, std::move(rightSymbols));
    return id;
}

ProductionTable const &Grammar::getProductionTable() const {
    return experimentProdTbl;
}

String Grammar::dump() const {
    String s;
    auto &symVec = getAllSymbols();
    auto &rules = getProductions();
    const char *typeStr[] = {"  Nonterminals:\n", "  Terminals:\n"};
    vector<int> classifiedSyms[2];

    for (auto &sym : symVec) {
        classifiedSyms[(int)sym.type].push_back(sym.id);
    }
    s += "Symbols:\n";
    for (int i = 1; i >= 0; --i) {
        s += typeStr[i];
        for (int symid : classifiedSyms[i]) {
            auto &sym = symVec[symid];
            s += "    { name: \"" + sym.name + "\", ";
            s += "id: " + std::to_string(sym.id) + " }";
            if (sym.id == getStartSymbol().id) {
                s += " [START]";
            }
            s += '\n';
        }
    }

    s += "Rules:\n";
    for (auto &ruleEntry : rules) {
        auto &rule = ruleEntry.second;
        int nid = ruleEntry.first; // Nonterminal id

        if (rule.empty())
            continue; // No rule for this symbol
        s += "  " + symVec[nid].name + " ->";
        bool firstRule = true;
        for (auto &ruleItem : rule) {
            if (!firstRule) {
                s += "    |";
            }
            for (int ruleRhs : ruleItem) {
                s += ' ';
                s += symVec[ruleRhs].name;
            }
            s += '\n';
            firstRule = false;
        }
    }

    return s;
}

auto Grammar::fromFile(const char *fileName) -> Grammar {
    std::fstream stream(fileName, std::ios::in);
    if (!stream.is_open()) {
        throw std::runtime_error(String("File not found: ") + fileName);
    }
    return GrammarReader::parse(stream).fillSymbolAttrs();
}

auto Grammar::fromStdin() -> Grammar {
    return GrammarReader::parse(::std::cin).fillSymbolAttrs();
}

void Grammar::checkViolcations() {
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

// This function needs to get a Symbol & from symvec,
// so symvec must be a mutable reference
static bool resolveNullable(Grammar::symvec_t &symvec,
                            Grammar::prodvt_t const &prodVecTable,
                            Symbol &sym) {
    if (sym.nullable.has_value()) {
        return sym.nullable.value();
    }

    // Place false first to prevent infinite recursive calls
    sym.nullable.emplace(false);

    // For t in T, t is not nullable
    if (sym.type == SymbolType::TERM) {
        return false;
    }

    // For A -> a...b, A is nullable <=> a ... b are all nullable
    for (auto const &prod : prodVecTable.at(sym.id)) {
        // This symbol has epsilon production
        if (prod.empty()) {
            sym.nullable.emplace(true);
            return true;
        }
        bool prodNullable = true;
        for (int component : prod) {
            if (!resolveNullable(symvec, prodVecTable, symvec[component])) {
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

// This function needs to get a Symbol & from symvec,
// so symvec must be a mutable reference
static void resolveFirstSet(vector<int> &visit, Grammar::symvec_t &symvec,
                            Grammar::prodvt_t const &prodVecTable, Symbol &sym,
                            int epsId) {
    if (visit[sym.id])
        return;

    // Mark the flag to prevent circular recursive call
    visit[sym.id] = 1;

    auto const &prods = prodVecTable.at(sym.id);
    for (auto const &prod : prods) {
        bool allNull = true;
        for (int symidInProd : prod) {
            auto &symInProd = symvec[symidInProd];
            if (!visit[symidInProd]) {
                resolveFirstSet(visit, symvec, prodVecTable, symInProd, epsId);
            }
            for (int sid : symInProd.firstSet) {
                if (sid != epsId)
                    sym.firstSet.insert(sid);
            }
            if (!symInProd.nullable.value()) {
                allNull = false;
                break;
            }
        }
        if (allNull) {
            sym.firstSet.insert(epsId);
        }
    }
}

// This function figures out the dependencies among Follow sets.
static void
resolveFollowSet(vector<int> &visit, Grammar::symvec_t &symvec,
                 unordered_map<int, unordered_set<int>> &dependencyTable,
                 std::pair<const int, std::unordered_set<int>> &entry) {
    if (visit[entry.first]) {
        return;
    }
    visit[entry.first] = 1;

    auto &parentSet = entry.second;
    for (int parent : parentSet) {
        auto it = dependencyTable.find(parent);
        if (it != dependencyTable.end() && !it->second.empty()) {
            resolveFollowSet(visit, symvec, dependencyTable, *it);
        }
        // Add follow set items from parent
        auto const &parentFollowSet = symvec[parent].followSet;
        auto &followSet = symvec[entry.first].followSet;
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
        resolveNullable(symVec, prodVecTable, symbol);
    }

    display(DisplayType::SYMBOL_TABLE, "Calculate nullables", this);

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
        resolveFirstSet(visit, symVec, prodVecTable, symbol, epsilon);
    }

    display(DisplayType::SYMBOL_TABLE, "Calculate first set", this);

    //--------------- Follow Set ---------------
    // Add $ to Follow set of the start symbol
    symVec[start].followSet.insert(endOfInput);

    // Get symbols from the next symbol's First set, and generate
    // a dependency graph.
    // e.g. table[a] = {b, c} ===> a needs b and c
    unordered_map<int, unordered_set<int>> dependencyTable;

    for (auto const &entry : prodVecTable) {
        long int product = entry.first;
        auto const &prods = entry.second;

        for (auto const &prod : prods) {
            // Skip epsilon productions
            if (prod.empty())
                continue;

            auto const &last = symVec[prod.back()];
            if (last.type == SymbolType::NON_TERM)
                dependencyTable[last.id].insert(product);

            long prodlen = static_cast<long>(prod.size());
            for (long i = prodlen - 2; i >= 0; --i) {
                // Only calcuate Follow sets for non-terminals
                if (symVec[prod[i]].type != SymbolType::NON_TERM)
                    continue;

                for (int first : symVec[prod[i + 1]].firstSet) {
                    if (first != epsilon)
                        symVec[prod[i]].followSet.insert(first);
                }
                if (symVec[prod[i + 1]].nullable.value())
                    dependencyTable[prod[i]].insert(prod[i + 1]);
            }
        }
    }

    // Figure out dependencies
    std::fill(visit.begin(), visit.end(), 0);
    for (auto &entry : dependencyTable) {
        resolveFollowSet(visit, symVec, dependencyTable, entry);
    }

    display(DisplayType::SYMBOL_TABLE, "Calculate follow set", this);

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

String Grammar::dumpNullable(const Symbol &symbol) const {
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

} // namespace gram

// Syntax Reader
namespace gram {

Grammar GrammarReader::parse(istream &is) {
    Grammar g;
    GrammarReader reader(is);
    reader.parse(g);
    return g;
}

void GrammarReader::parse(Grammar &g) try {
    String s;

    expectOrThrow("TERM");
    expectOrThrow(":");
    expectOrThrow("{");

    // Start T definition
    while (getToken(s)) {
        g.putSymbol(s.c_str(), true);
        if (!expect(','))
            break;
    }

    expectOrThrow("}");

    // Start N definition
    expectOrThrow("START:");
    if (!getToken(s, false)) {
        throw std::runtime_error("Please provide a start symbol");
    }
    g.setStart(s.c_str());
    if (getToken(s, false)) {
        throw std::runtime_error("Cannot define more than one start symbol");
    }

    while (getToken(s)) { // Has more rules
        int nid = g.putSymbol(s.c_str(), false);
        std::vector<std::vector<int>> rule;

        expectOrThrow("->");
        do {
            std::vector<int> ruleRhs;
            bool hasEpsilon = false;
            while (getToken(s, false)) {
                // put symbol and delay checking
                int symid = g.putSymbolUnchecked(s.c_str());
                ruleRhs.push_back(symid);
                if (symid == g.getEpsilonSymbol().id) {
                    hasEpsilon = true;
                }
            }
            if (ruleRhs.empty()) {
                throw std::runtime_error(
                    "No token found in right side of the rule");
            }
            if (hasEpsilon && ruleRhs.size() > 1) {
                throw std::runtime_error("Epsilon cannot be used along with "
                                         "other symbols in the same rule");
            }
            if (hasEpsilon) {
                ruleRhs.clear(); // Epsilon rule
            }
            rule.push_back(std::move(ruleRhs));
        } while (expect('|'));

        g.addRule(nid, std::move(rule));
    }

    // Check redundant input (which normally means invalid syntax)
    const char *e = nullptr;
    if (!token.empty())
        e = token.c_str();
    if (skipSpaces(pos))
        e = pos;
    if (e)
        throw std::runtime_error(String("Redunant input: ") + e);

    g.checkViolcations();

} catch (UnsolvedSymbolError &e) {
    String s = "Parsing error at line " +
               std::to_string(tokenLineNo[e.symInQuestion.name]) + ": " +
               e.what();
    throw std::runtime_error(s);

} catch (std::runtime_error &e) {
    String s = "Parsing error at line ";
    s += std::to_string(linenum);
    s += ", char ";
    auto offset = pos - lineStart + 1;
    if (offset > 0)
        s += std::to_string(offset);
    else
        s += "<Unknown>";
    s += ": ";
    s += e.what();
    throw std::runtime_error(s);
}

// Wrapper of getline
auto GrammarReader::getLineAndCount(istream &is, String &s) -> bool {
    if (std::getline(is, s)) {
        lineStart = s.c_str();
        ++linenum;
        return true;
    }
    return false;
}

static bool isCommentStart(char ch) { return ch == '!' || ch == '#'; }

// Make sure *p is non-space
// This is the only method to use `stream` directly, expect for
// getLineAndCount
auto GrammarReader::skipSpaces(const char *p) -> const char * {
    if (!p)
        return nullptr;
    while (true) {
        while (*p && isspace(*p))
            ++p;
        if (*p && !isCommentStart(*p))
            return p;
        // A comment start ('!') mark is equal to end of line
        // End of line: we should refetch string
        if (!getLineAndCount(stream, line))
            return nullptr;
        p = line.c_str();
    }
    throw std::logic_error("Unreachable code");
}

// Make sure *p is non-blank
// This is the only method to use `stream` directly, expect for
// getLineAndCount
auto GrammarReader::skipBlanks(const char *p) -> const char * {
    if (!p)
        return nullptr;
    // Do not fetch new line
    while (*p && isblank(*p))
        ++p;
    if (isCommentStart(*p)) {
        while (*p)
            ++p;
    }
    return p;
}

// Compare next non-space char with `ch`
auto GrammarReader::nextEquals(char ch) -> bool {
    if (!token.empty())
        return token[0] == ch;
    // No exceptions happen here, so we do not need to preserve pos
    pos = skipSpaces(pos);
    if (!pos)
        return false;
    return *pos == ch;
}

// Compare next non-space char with `ch`, and consume it if they are equal
auto GrammarReader::expect(char ch) -> bool {
    if (!token.empty()) {
        if (token[0] == ch) {
            token = token.substr(1);
            return true;
        }
        return false;
    }
    pos = skipSpaces(pos);
    if (!pos || *pos != ch)
        return false;
    ++pos;
    return true;
}

// Compare next non-space sequence with `expected`,
// and consume it if they are equal. Otherwise, an exception is thrown.
auto GrammarReader::expectOrThrow(const char *expected) -> void {
    const char *expectedStart = expected;
    const char *check = skipSpaces(pos);

    if (!check) {
        String s = "Rules are incomplete: Expecting \"";
        s += expectedStart;
        s += "\"";
        throw std::runtime_error(s);
    }
    while (*check && *expected && *check == *expected) {
        ++check;
        ++expected;
    }
    if (*expected) { // expected string should be exhausted but is not
        String s = "Characters do not match: Expecting \"";
        s += expectedStart;
        s += "\"";
        throw std::runtime_error(s);
    }

    pos = check;
}

// Try to get a token (This process may fail).
// This process is diffcult because C++ does not have a Scanner
// like Java, and buffer needs to be managed by ourselves.
auto GrammarReader::getToken(String &s, bool newlineAutoFetch) -> bool {
    if (!token.empty()) {
        s = token;
        token.clear();
        return true;
    }

    const char *p = newlineAutoFetch ? skipSpaces(pos) : skipBlanks(pos);
    if (!p)
        return false;

    // Check the first character
    // There're 4 cases: backslash quote digit(invalid) _alpha
    if (std::isdigit(*p)) {
        throw std::runtime_error(
            "The first character of a token cannot be a digit");
    }

    s.clear();

    if (*p == '\'' || *p == '\"') {
        char quoteChar = *p; // Quote
        const char *cur = p + 1;
        for (; *cur && *cur != quoteChar; ++cur) {
            continue;
        }

        // Change pos so error report is more precise.
        pos = cur;
        if (*cur != quoteChar) {
            throw std::runtime_error(
                String("Cannot find matching quote pair ") + quoteChar);
        }

        // [p, pos] ===> [" ... "]
        // Advance pos so we don't get overlapped scanning results
        s.append(p + 1, pos++);
        tokenLineNo[s] = linenum;
        return true;
    }

    // Allow `\` at the beginning so escaping sequences can work
    if (*p == '\\')
        s += *p++;

    for (; *p && (std::isalnum(*p) || *p == '_'); ++p) {
        s += *p;
    }

    // Even if s is empty, we may have skipped some spaces,
    // which saves time for later work
    pos = p;

    if (!s.empty()) {
        tokenLineNo[s] = linenum;
        return true;
    }
    return false;
}

auto GrammarReader::ungetToken(const String &s) -> void {
    if (!token.empty()) {
        throw std::logic_error("Number of ungot tokens > 1");
    }
    token = s;
}

} // namespace gram