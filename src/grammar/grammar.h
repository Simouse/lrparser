#ifndef __GRAMMAR_H__
#define __GRAMMAR_H__

#include <optional>
#include <stdexcept>
#include <unordered_map>
// #include <unordered_set>
#include <set>
#include <vector>

#include "src/common.h"

namespace gram {
class Automaton;
class Display;
} // namespace gram

namespace gram {
enum SymbolType {
    NON_TERM = 0,
    TERM = 1, // For bool comparison
    UNCHECKED
};

enum ProductionID : int;

struct Production {
    int leftSymbol;
    std::vector<int> rightSymbols;
    Production(int left, std::vector<int> right)
        : leftSymbol(left), rightSymbols(std::move(right)) {}
};

using ProductionTable = std::vector<Production>;

class Grammar;

struct Symbol {
    using symset_t = ::std::set<int>;
    std::optional<bool> nullable;
    SymbolType type;
    int id;
    String name;
    symset_t firstSet;
    symset_t followSet;

    Symbol(SymbolType type, int id, String name)
        : type(type), id(id), name(std::move(name)) {}
};

class UnsolvedSymbolError : public std::runtime_error {
  public:
    UnsolvedSymbolError(const Symbol &sym)
        : symInQuestion(sym), std::runtime_error("Unsolved symbol: " +
                                                 sym.name) {}
    const Symbol &symInQuestion;
};

class Grammar {
  public:
    using prodvt_t = std::unordered_map<int, std::vector<std::vector<int>>>;
    using symvec_t = std::vector<Symbol>;
    using idtbl_t = std::unordered_map<String, int>;

  private:
    friend class GrammarReader;
    int start = -1;
    int epsilon = -1;
    int endOfInput = -1;
    symvec_t symVec;
    idtbl_t idTable;
    prodvt_t prodVecTable;

    ProductionTable experimentProdTbl;
    ProductionID addProduction(int leftSymbol, std::vector<int> rightSymbols);

    int putSymbolNoDuplicate(Symbol &&sym); // Detect duplicates
    void checkViolcations();                // Throws if there're violations

    // Private constructor
    Grammar() {
        // Add built-in symbols
        epsilon = putSymbol(Grammar::SignStrings::epsilon, true);
        addAlias(epsilon, "_e");
        addAlias(epsilon, "\\e");
        addAlias(epsilon, "\\epsilon");
        endOfInput = putSymbol(Grammar::SignStrings::endOfInput, true);
    }

    void setStart(const char *name);
    int putSymbol(const char *name, bool isTerm);
    int putSymbolUnchecked(const char *name); // Delay checking
    void setEpsilon(int symid);
    void addAlias(int sid, const char *alias);
    void addRule(int nid, std::vector<std::vector<int>> &&rule);

  public:
    [[nodiscard]] symvec_t const &getAllSymbols() const { return symVec; }
    [[nodiscard]] prodvt_t const &getProductions() const {
        return prodVecTable;
    }
    [[nodiscard]] const Symbol &getStartSymbol() const {
        return symVec[start];
    };
    [[nodiscard]] const Symbol &getEpsilonSymbol() const {
        return symVec[epsilon];
    };
    [[nodiscard]] const Symbol &getEndOfInputSymbol() const {
        return symVec[endOfInput];
    };
    [[nodiscard]] ProductionTable const &getProductionTable() const;

    [[nodiscard]] String dump() const;
    [[nodiscard]] String dumpNullable(const Symbol &symbol) const;
    [[nodiscard]] String dumpFirstSet(const Symbol &symbol) const;
    [[nodiscard]] String dumpFollowSet(const Symbol &symbol) const;

    // Fill symbol attributes: nullable, firstSet, followSet
    Grammar &fillSymbolAttrs();

    // Factories
    static auto fromFile(const char *fileName) -> Grammar;
    static auto fromStdin() -> Grammar;

    struct SignStrings {
        static constexpr const char *dot = "\xE2\x97\x8F"; // "●" in UTF-8
        static constexpr const char *epsilon = "\xCE\xB5"; // "ε" in UTF-8
        static constexpr const char *endOfInput = "$";
    };
};
} // namespace gram

#endif