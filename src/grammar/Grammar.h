#ifndef LRPARSER_GRAM_H
#define LRPARSER_GRAM_H

#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/automata/Automaton.h"
#include "src/common.h"
#include "src/util/BitSet.h"

namespace gram {
class Automaton;
} // namespace gram

namespace gram {
enum SymbolType {
    NON_TERM = 0,
    TERM = 1, // For bool comparison
    UNCHECKED
};

enum ProductionID : int;
// enum SymbolID : int;
// We may use static_cast when an element needs it, but a container
// containing those elements cannot be cast. And allowing BitSet<T1> and
// BitSet<T2> to be calculated together sounds wired.
using SymbolID = ActionID;

struct Production {
    SymbolID leftSymbol;
    std::vector<SymbolID> rightSymbols;
    Production(SymbolID left, std::vector<SymbolID> right)
        : leftSymbol(left), rightSymbols(std::move(right)) {}
};

using ProductionTable = std::vector<Production>;

class Grammar;

struct Symbol {
    using SymbolSet = util::BitSet<SymbolID>;
    std::optional<bool> nullable;
    SymbolType type;
    SymbolID id;
    String name;
    // Productions that can generate this symbol
    std::vector<ProductionID> productions;
    std::vector<StateID> startStates;
    SymbolSet firstSet;
    SymbolSet followSet;

    Symbol(SymbolType type, SymbolID id, String name)
        : type(type), id(id), name(std::move(name)) {}
};

class Grammar {
  public:
    using symvec_t = std::vector<Symbol>;
    using idtbl_t = std::unordered_map<String, SymbolID>;

  private:
    friend class GrammarReader;
    SymbolID start{-1};
    SymbolID epsilon{-1};
    SymbolID endOfInput{-1};
    symvec_t symbolVector;
    idtbl_t idTable;
    ProductionTable productionTable;

    // Private constructor.
    // Grammar class is mostly private when building grammar, because we
    // only want GrammarReader to access its methods.
    Grammar();

    ProductionID addProduction(SymbolID leftSymbol,
                               std::vector<SymbolID> rightSymbols);

    // This method can detect duplicates. All symbol-putting methods should
    // eventually call this one.
    SymbolID putSymbolNoDuplicate(Symbol &&sym);

    SymbolID putSymbol(const char *name, bool isTerm);

    // This method can put symbol with delayed existence checking
    SymbolID putSymbolUnchecked(const char *name);

    // Throws if there are violations
    void checkViolations();

    void setStart(const char *name);

    void addAlias(SymbolID sid, const char *alias);

    // Recursively resolve Follow set dependency: a dependency table must be
    // built first.
    void resolveFollowSet(
        std::vector<int> &visit,
        std::unordered_map<SymbolID, std::unordered_set<SymbolID>>
            &dependencyTable,
        std::pair<const SymbolID, std::unordered_set<SymbolID>> &dependency);

    // Recursively resolve First set dependency
    void resolveFirstSet(std::vector<int> &visit, Symbol &curSymbol);

    // Recursively resolve nullable dependency
    bool resolveNullable(Symbol &sym);

  public:
    [[nodiscard]] symvec_t const &getAllSymbols() const;
    [[nodiscard]] const Symbol &getStartSymbol() const;
    [[nodiscard]] const Symbol &getEpsilonSymbol() const;
    [[nodiscard]] const Symbol &getEndOfInputSymbol() const;
    [[nodiscard]] ProductionTable const &getProductionTable() const;
    [[nodiscard]] String dump() const;
    [[nodiscard]] static String dumpNullable(const Symbol &symbol);
    [[nodiscard]] String dumpFirstSet(const Symbol &symbol) const;
    [[nodiscard]] String dumpFollowSet(const Symbol &symbol) const;
    [[nodiscard]] String dumpProduction(ProductionID prodID) const;

    // std::unordered_map doesn't support heterogeneous lookup, so when
    // we pass a const char *, the string is copied. So we just use a string
    // const & to avoid copy when we already have a string...
    [[nodiscard]] Symbol const &findSymbol(String const &s) const;

    // Fill symbol attributes: nullable, firstSet, followSet
    Grammar &resolveSymbolAttributes();

    // Factories
    static auto fromFile(const char *fileName) -> Grammar;
    static auto fromStdin() -> Grammar;

    struct SignStrings {
        static constexpr const char *dot = "\xE2\x97\x8F"; // "●" in UTF-8
        static constexpr const char *epsilon = "\xCE\xB5"; // "ε" in UTF-8
        static constexpr const char *endOfInput = "$";
    };

    class UnsolvedSymbolError : public std::runtime_error {
      public:
        explicit UnsolvedSymbolError(const Symbol &sym)
            : std::runtime_error("Unsolved symbol: " + sym.name),
              symInQuestion(sym) {}
        const Symbol &symInQuestion;
    };

    // This error is thrown if we try to look up a symbol which does not exist
    // after grammar is built.
    class NoSuchSymbolError : public std::runtime_error {
      public:
        explicit NoSuchSymbolError(String const &name)
            : std::runtime_error("No such symbol: " + name) {}
    };
};
} // namespace gram

#endif