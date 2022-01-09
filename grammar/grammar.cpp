#include "grammar/grammar.h"
#include <cctype>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using std::ifstream;
using std::istream;
using std::string;
using std::vector;

namespace lr {

// TODO: add \e support
// TODO: add comment support
// TODO: add 'c' support
// TODO: add "str" support
// TODO: check if all terminals in rule can be found
// TODO: check if a symbol is redefined but types are different
// TODO: check if unnecessary terminals exist
// TODO: convert fstream to universe istream

class GrammerReader {
  private:
    int linenum = 0, parsed = 0;
    // Make sure pos is empty but not NULL in the beginning
    const char *pos = "";
    // Only valid if pos != NULL and stream is read at least once
    const char *lineStart;
    std::string line, token;
    std::istream &stream;
    std::unordered_map<std::string, int> tokenLineNo;

    auto getlineAndCount(std::istream &is, std::string &s) -> bool;
    auto skipSpaces(const char *p) -> const char *;
    auto skipBlanks(const char *p) -> const char *;
    auto nextEquals(char ch) -> bool;
    auto expect(char ch) -> bool;
    void expectOrThrow(const char *expected);
    auto getToken(std::string &s, bool multiline = true) -> bool;
    void ungetToken(const std::string &s);

  public:
    void parse(Grammar &g);
    GrammerReader(std::istream &is) : stream(is) {}
};

void Grammar::build() {
    checkViolcations();

    // Clear unncessary data
    uncheckedSyms.clear();
    buildFlag = true;
}

void Grammar::setEpsilon(int symid) { epsilonSym = symid; }

bool Grammar::isEpsilonRule(int nid, int rhsIndex) const {
    return rules.at(nid)[rhsIndex].empty();
}

auto Grammar::putSymbolNoDuplicate(Symbol &&sym) -> int {
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
    // Do not rely on `id` in arg `sym`
    int symid = (int)symVec.size();
    idTable.emplace(sym.name, symid);
    // Use `move` after sym.name is copyed to idTable
    symVec.push_back(std::move(sym));
    return symid;
}

auto Grammar::putSymbol(const char *name, bool isTerm) -> int {
    if (buildFlag) {
        throw BuiltGrammarModificationError();
    }
    Symbol sym{isTerm ? SymType::TERM : SymType::NONTERM, (int)symVec.size(),
               name};
    return putSymbolNoDuplicate(std::move(sym));
}

// Delays checking
auto Grammar::putSymbolUnchecked(const char *name) -> int {
    if (buildFlag) {
        throw BuiltGrammarModificationError();
    }
    Symbol sym{SymType::UNCHECKED, (int)symVec.size(), name};
    return putSymbolNoDuplicate(std::move(sym));
}

void Grammar::addAlias(int id, const char *alias) {
    if (buildFlag) {
        throw BuiltGrammarModificationError();
    }
    if (symVec.size() <= id) {
        throw std::runtime_error("No such symbol: " + std::to_string(id));
    }
    idTable.emplace(alias, id);
}

void Grammar::addRule(int nid, vector<vector<int>> &&newRule) {
    if (buildFlag) {
        throw BuiltGrammarModificationError();
    }
    rules.emplace(nid, std::move(newRule));
}

auto Grammar::dump() const -> std::string {
    string s;
    auto &symVec = getAllSymbols();
    auto &rules = getRules();
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
    Grammar g; // Return value optimization

    std::fstream stream(fileName, std::ios::in);
    if (!stream.is_open()) {
        throw std::runtime_error(string("File not found: ") + fileName);
    }
    GrammerReader reader(stream);
    reader.parse(g);
    return g;
}

auto Grammar::fromStdin() -> Grammar {
    Grammar g;

    GrammerReader reader(std::cin);
    reader.parse(g);
    return g;
}

void Grammar::checkViolcations() {
    // Check if there's a sym with no type
    for (auto &sym : symVec) {
        if (sym.type == SymType::UNCHECKED) {
            throw UnsolvedSymbolError(sym);
        }
    }
}

auto Grammar::getAllSymbols() const -> const decltype(symVec) & {
    return this->symVec;
}

auto Grammar::getRules() const -> const decltype(rules) & {
    return this->rules;
}

auto Grammar::getSymbol(int id) const -> const Symbol & {
    return this->symVec[id];
}

void Grammar::setStart(const char *name) {
    if (buildFlag) {
        throw BuiltGrammarModificationError();
    }
    // Although we know start symbol must not be a terminal,
    // we cannot define it here.
    startSym = putSymbolUnchecked(name);
}

auto Grammar::getStartSymbol() const -> const Symbol & {
    return symVec[startSym];
}

auto Grammar::getEpsilonSymbol() const -> const Symbol & {
    return symVec[epsilonSym];
}

string Grammar::getLR0StateName(int nid, int rhsIndex, int dotPos) const {
    auto dotSign = getDotSign();
    auto &rule = rules.at(nid);     // Rule of this symbol
    auto &ruleRhs = rule[rhsIndex]; // Rhs(s) of this rule

    string s = symVec[nid].name;
    s += " ->";

    int i = 0; // Symbols appended
    for (; i < dotPos; ++i) {
        s += ' ';
        s += symVec[ruleRhs[i]].name;
    }
    s += ' ';
    s += dotSign;
    for (; i < ruleRhs.size(); ++i) {
        s += ' ';
        s += symVec[ruleRhs[i]].name;
    }
    return s;
}

} // namespace lr

// Syntax Reader
namespace lr {

void GrammerReader::parse(Grammar &g) try {
    string s;

    if (parsed != 0) {
        throw std::runtime_error("Method parseRule() can only be called once");
    }

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
    expectOrThrow("!");

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
        throw std::runtime_error(string("Redunant input: ") + e);

    g.build();

    parsed = 1;
} catch (UnsolvedSymbolError &e) {
    string s = "Parsing error at line " +
               std::to_string(tokenLineNo[e.symInQuestion.name]) + ": " +
               e.what();
    throw std::runtime_error(s);
} catch (std::runtime_error &e) {
    string s = "Parsing error at line ";
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
auto GrammerReader::getlineAndCount(istream &is, string &s) -> bool {
    if (std::getline(is, s)) {
        lineStart = s.c_str();
        ++linenum;
        return true;
    }
    return false;
}

// Make sure *p is non-space
// This is the only method to use `stream` directly, expect for
// getlineAndCount
auto GrammerReader::skipSpaces(const char *p) -> const char * {
    if (!p)
        return nullptr;
    while (true) {
        while (*p && isspace(*p))
            ++p;
        if (*p)
            return p;
        // End of line: we should refetch string
        if (!getlineAndCount(stream, line))
            return nullptr;
        p = line.c_str();
    }
    throw std::logic_error("Unreachable code");
}

// Make sure *p is non-blank
// This is the only method to use `stream` directly, expect for
// getlineAndCount
auto GrammerReader::skipBlanks(const char *p) -> const char * {
    if (!p)
        return nullptr;
    // Do not fetch new line
    while (*p && std::isblank(*p))
        ++p;
    return p;
}

// Compare next non-space char with `ch`
auto GrammerReader::nextEquals(char ch) -> bool {
    if (!token.empty())
        return token[0] == ch;
    // No exceptions happen here so we do not need to preserve pos
    pos = skipSpaces(pos);
    if (!pos)
        return false;
    return *pos == ch;
}

// Compare next non-space char with `ch`, and consume it if they are equal
auto GrammerReader::expect(char ch) -> bool {
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
auto GrammerReader::expectOrThrow(const char *expected) -> void {
    const char *expectedStart = expected;
    const char *check = skipSpaces(pos);

    if (!check) {
        string s = "Rules are incomplete: Expecting \"";
        s += expectedStart;
        s += "\"";
        throw std::runtime_error(s);
    }
    while (*check && *expected && *check == *expected) {
        ++check;
        ++expected;
    }
    if (*expected) { // expected string should be exhausted but is not
        string s = "Characters do not match: Expecting \"";
        s += expectedStart;
        s += "\"";
        throw std::runtime_error(s);
    }

    pos = check;
}

// TODO: take care of special cases: \e.
// Try to get a token (This process may fail).
// This process is diffcult because C++ does not have a Scanner
// like Java, and buffer needs to be managed by ourselves.
auto GrammerReader::getToken(string &s, bool multiline) -> bool {
    if (!token.empty()) {
        s = token;
        token.clear();
        return true;
    }

    const char *p = multiline ? skipSpaces(pos) : skipBlanks(pos);
    if (!p)
        return false;
    if (std::isdigit(*p)) {
        throw std::runtime_error(
            "The first character of a token cannot be a digit");
    }
    s.clear();
    for (; *p && (std::isalnum(*p) || *p == '_'); ++p) {
        s += *p;
    }
    pos = p;
    if (!s.empty()) {
        tokenLineNo[s] = linenum;
        return true;
    }
    return false;
}

auto GrammerReader::ungetToken(const string &s) -> void {
    if (!token.empty()) {
        throw std::logic_error("Number of ungot tokens > 1");
    }
    token = s;
}

} // namespace lr