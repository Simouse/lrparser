#ifndef LRPARSER_LR_H
#define LRPARSER_LR_H

#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <istream>
#include <memory>
#include <unordered_map>
#include <vector>

#include "src/automata/PushDownAutomaton.h"
#include "src/grammar/Grammar.h"
#include "src/util/BitSet.h"
#include "src/util/ResourceProvider.h"
#include "src/util/TokenReader.h"

namespace gram {

class Grammar;

class LRParser : public util::ResourceProvider<TransitionSet> {
  public:
    struct ParseAction {
        enum Type { GOTO, SHIFT, REDUCE, SUCCESS };
        Type type;
        union {
            StateID dest;
            ProductionID productionID;
            int untyped_data;
        };

        static_assert(sizeof(dest) == sizeof(productionID) &&
                      sizeof(dest) == sizeof(untyped_data));

        ParseAction(Type t, int data) : type(t), untyped_data(data) {}

        // For putting into a set.
        bool operator<(ParseAction const &other) const {
            if (type != other.type) {
                return (int)type < (int)other.type;
            }
            return untyped_data < other.untyped_data;
        }
    };

    // Store actionTable and gotoTable in the same place.
    // Unfortunately, C++ doesn't have dynamic high-dimensional arrays, so
    // we have to use std::vector instead.
    using ParseTable = ::std::vector<std::vector<std::set<ParseAction>>>;

    // Stores data to generate states from a symbol's productions
    using StateSeed = std::pair<SymbolID, Constraint *>;

    explicit LRParser(const gram::Grammar &g)
        : gram(g), nfa(this, &this->kernelLabelMap),
          dfa(this, &this->kernelLabelMap) {}

    virtual void buildNFA();
    virtual void buildDFA();
    void buildParseTable();
    bool test(::std::istream &stream);

    // Accessors
    [[nodiscard]] auto const &getParseTable() const { return parseTable; }
    [[nodiscard]] auto const &getGrammar() const { return gram; }
    [[nodiscard]] auto const &getNFA() const { return nfa; }
    [[nodiscard]] auto const &getDFA() const { return dfa; }
    [[nodiscard]] auto const &getStateStack() const { return stateStack; }
    [[nodiscard]] auto const &getInputQueue() const { return InputQueue; }
    [[nodiscard]] auto const &getSymbolStack() const { return symbolStack; }
    [[nodiscard]] bool hasMoreInput() const { return inputFlag; }

    // Format
    [[nodiscard]] std::string dumpParseTableEntry(StateID state,
                                                  ActionID action) const;

    // Implementation of interface util::ResourceProvider<TransitionSet>.
    // With this interface automatons can get transition resources whose
    // lifetime can be longer than itself.
    TransitionSet *requestResource() override {
        transitionSetPool.push_back(std::make_unique<TransitionSet>());
        return transitionSetPool.back().get();
    }

  protected:
    bool inputFlag = true;
    // PDA final state ID (not closure ID). Used to put SUCCESS entry. Assigned
    // in buildNFA(). Since LALR uses a different building method, this should
    // be assigned in LALR's buildDFA().
    StateID auxEnd{-1};
    const gram::Grammar &gram;
    PushDownAutomaton nfa; // Built in buildNFA()
    PushDownAutomaton dfa; // Built in buildDFA()
    ParseTable parseTable; // Built in buildParseTable()
    std::deque<SymbolID> InputQueue;
    std::vector<StateID> stateStack;
    std::vector<SymbolID> symbolStack;
    // Fetch kernel label by productionID and rhsIndex.
    // The shape of this map (not square) is important to the following process.
    // The last production is S' -> S, which is added automatically.
    std::vector<std::vector<const char *>> kernelLabelMap;
    std::vector<std::unique_ptr<Constraint>> constraintPool;
    std::vector<std::unique_ptr<char[]>> stringPool;
    std::vector<std::unique_ptr<TransitionSet>> transitionSetPool;
    std::set<std::pair<int, int>> parseTableConflicts;
    // Contains all non-epsilon terminals. Built in buildKernel().
    Constraint *allTermConstraint = nullptr;

    // Called at the beginning of buildNFA().
    void buildKernel();

    // Creates a new constraint and store it in the pool.
    // Returns the pointer to the stored constraint.
    Constraint *newConstraint(size_t size) {
        constraintPool.push_back(
            std::make_unique<Constraint>(size));
        return constraintPool.back().get();
    }

    // Stores an existing constraint in the pool.
    // Returns the pointer to the stored constraint.
    Constraint *newConstraint(Constraint constraints) {
        constraintPool.push_back(
            std::make_unique<Constraint>(std::move(constraints)));
        return constraintPool.back().get();
    }

    // Copies a string, and stores it the pool.
    // Returns the pointer to the stored C-style string.
    const char *newString(std::string const &s) {
        auto holder = std::make_unique<char[]>(s.size() + 1);
        char *buf = holder.get();
        strcpy(buf, s.c_str());
        stringPool.push_back(std::move(holder));
        return buf;
    }

    // The returned resource should be allocated by newConstraint(), which
    // keeps the resource available until parser is destroyed.
    // Argument `parentConstraint` may be nullptr.
    virtual Constraint *
    resolveLocalConstraints(Constraint const *parentConstraint,
                            Production const &production, int rhsIndex) = 0;

    // Whether the automaton states should contain constraints (lookaheads) in
    // their labels. Default: LR0 (ignore constraints).
    [[nodiscard]] virtual bool shouldDumpConstraint() const { return false; }

    // Base implementation: ignore constraint.
    [[nodiscard]] virtual std::function<size_t(StateSeed const &seed)>
    getSeedHashFunc() const {
        return [](StateSeed const &seed) -> std::size_t {
            return std::hash<int>()(seed.first);
        };
    }

    // Base implementation: ignore constraint.
    [[nodiscard]] virtual std::function<bool(StateSeed const &a,
                                             StateSeed const &b)>
    getSeedEqualFunc() const {
        return [](StateSeed const &s1, StateSeed const &s2) -> bool {
            return s1.first == s2.first;
        };
    }

  private:
    // Read next symbol from the given stream.
    // This is only used inside test() method.
    void readSymbol(util::TokenReader &reader);

    // Creates the table if it does not exist, and add an entry to
    // it.
    void addParseTableEntry(StateID state, ActionID act, ParseAction pact);

    // Try to apply reduction by production with the given ID. Throws an error
    // if reduction fails.
    void reduce(ProductionID prodID);
};

} // namespace gram

#endif