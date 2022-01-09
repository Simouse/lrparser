#ifndef __GRAMMAR_H__
#define __GRAMMAR_H__

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace am {
class Automaton;
}

namespace lr {
enum SymType {
    NONTERM = 0,
    TERM = 1, // For bool comparison
    UNCHECKED
};

struct Symbol {
    SymType type;
    int id;
    std::string name;
};

class UnsolvedSymbolError : public std::runtime_error {
  public:
    UnsolvedSymbolError(const Symbol &sym)
        : symInQuestion(sym), std::runtime_error("Unsolved symbol: " +
                                                 sym.name) {}
    // [[nodiscard]] const char* what() const noexcept override;
    const Symbol &symInQuestion;
};

class BuiltGrammarModificationError : public std::runtime_error {
  public:
    BuiltGrammarModificationError()
        : std::runtime_error("Cannot modify a built grammar") {}
};

class Grammar {
    // A empty vector<int> rule item is epison
  private:
    bool buildFlag = false;
    int startSym = -1;
    int epsilonSym = -1;
    std::vector<Symbol> symVec;
    std::vector<int> uncheckedSyms; // Clear when grammar is built
    std::unordered_map<std::string, int> idTable;
    std::unordered_map<int, std::vector<std::vector<int>>> rules;

    int putSymbolNoDuplicate(Symbol &&sym); // Detect duplicates
    void checkViolcations();                // Throws if there're violations

    Grammar() { // Add epsilon as a built-in symbol
        int eid = putSymbol("_e", true);
        addAlias(eid, "\\e");
        addAlias(eid, "\\epsilon");
        setEpsilon(eid);
    }

  public:
    // BuildFlag should be false when calling those:
    void setStart(const char *name);
    int putSymbol(const char *name, bool isTerm);
    int putSymbolUnchecked(const char *name); // Delay checking
    void setEpsilon(int symid);
    void addAlias(int sid, const char *alias);
    void addRule(int nid, std::vector<std::vector<int>> &&rule);

    void build();

    // TODO: move definitions of short getters here
    [[nodiscard]] auto getSymbol(int id) const -> const Symbol &;
    [[nodiscard]] auto getAllSymbols() const -> const decltype(symVec) &;
    [[nodiscard]] auto getRules() const -> const decltype(rules) &;
    [[nodiscard]] auto getStartSymbol() const -> const Symbol &;
    [[nodiscard]] auto getEpsilonSymbol() const -> const Symbol &;
    [[nodiscard]] auto getDotSign() const -> const char * { return "●"; }
    [[nodiscard]] auto getLR0StateName(int nid, int rhsIndex, int dotPos) const
        -> std::string;
    [[nodiscard]] bool isEpsilonRule(int nid, int rhsIndex) const;

    [[nodiscard]] std::string dump() const;

    // Factories:
    static auto fromFile(const char *fileName) -> Grammar;
    static auto fromStdin() -> Grammar;
};
} // namespace lr

#endif