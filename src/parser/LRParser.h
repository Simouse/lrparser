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

#include "src/automata/Automaton.h"
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

        // For putting into a set
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
    using StateSeed = std::pair<SymbolID, util::BitSet<ActionID> *>;

    explicit LRParser(const gram::Grammar &g) : gram(g), nfa(this), dfa(this) {}

    virtual void buildNFA();
    virtual void buildDFA();
    void buildParseTable();
    // Test given stream with parsed results
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

    // With this interface automatons can get transition resources whose
    // lifetime can be longer than itself
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
    Automaton nfa;         // Build in buildNFA()
    Automaton dfa;         // Build in buildDFA()
    ParseTable parseTable; // Built in buildParseTable()
    std::deque<SymbolID> InputQueue;
    std::vector<StateID> stateStack;
    std::vector<SymbolID> symbolStack;
    // [productionID][index] ==> label.
    std::vector<std::vector<const char *>> kernelLabelMap;
    std::vector<std::unique_ptr<util::BitSet<ActionID>>> constraintPool;
    std::vector<std::unique_ptr<char[]>> stringPool;
    std::vector<std::unique_ptr<TransitionSet>> transitionSetPool;
    // Contains all non-epsilon terminals. Built in buildKernel().
    util::BitSet<ActionID> *allTermConstraint = nullptr;

    void buildKernel();

    util::BitSet<ActionID> *newConstraint(size_t size) {
        constraintPool.push_back(
            std::make_unique<util::BitSet<ActionID>>(size));
        return constraintPool.back().get();
    }

    util::BitSet<ActionID> *newConstraint(util::BitSet<ActionID> constraints) {
        constraintPool.push_back(
            std::make_unique<util::BitSet<ActionID>>(std::move(constraints)));
        return constraintPool.back().get();
    }

    // Copy a string, and stores it internally
    const char *newString(std::string const &s) {
        auto holder = std::make_unique<char[]>(s.size() + 1);
        char *buf = holder.get();
        strcpy(buf, s.c_str());
        stringPool.push_back(std::move(holder));
        return buf;
    }

    // The returned resource should be allocated by newConstraint(), which
    // keeps the resource available until parser is destroyed.
    // Because newConstraint() will allocate new resource, this function is not
    // const.
    // Argument `parentConstraint` may be nullptr.
    virtual util::BitSet<ActionID> *
    resolveLocalConstraints(util::BitSet<ActionID> const *parentConstraint,
                            Production const &production, int rhsIndex) = 0;

    // Virtual functions.
    // These functions change behaviors of the parser, and must be overridden if
    // buildNFA() and buildDFA() are not.

    // This method checks if current entry is applicable to add to the table.
    [[nodiscard]] virtual bool
    canAddParseTableEntry(StateID state, ActionID act, ParseAction pact,
                          StateID subState) const = 0;

    // Decides if the chain of this seed is final.
    [[nodiscard]] virtual bool
    canMarkFinal(StateSeed const &seed, Production const &production) const = 0;

    [[nodiscard]] virtual bool shouldIncludeConstraintsInDump() const = 0;

    // Only LR1 has to override this method
    [[nodiscard]] virtual std::function<size_t(StateSeed const &seed)>
    getSeedHashFunc() const {
        return [](StateSeed const &seed) -> std::size_t {
            return std::hash<int>()(seed.first);
        };
    }

    // Only LR1 has to override this method
    [[nodiscard]] virtual std::function<bool(StateSeed const &a,
                                             StateSeed const &b)>
    getSeedEqualFunc() const {
        return [](StateSeed const &s1, StateSeed const &s2) -> bool {
            return s1.first == s2.first;
        };
    }

    // These two functions are used by automaton, and have default values. Only
    // LALR will override them.
    // [[nodiscard]] virtual Automaton::ClosureEqualFuncType
    // getClosureEqualFunc() const;

    // [[nodiscard]] virtual Automaton::DuplicateClosureHandlerType
    // getDuplicateClosureHandler() const;

  private:
    // Read next symbol from the given stream.
    // This is only used inside test() method.
    void readSymbol(util::TokenReader &reader);

    // This method creates the table if it does not exist, and add an entry to
    // it.
    void addParseTableEntry(StateID state, ActionID act, ParseAction pact);

    // Throws an error if reduction fails.
    void reduce(ProductionID prodID);
};

} // namespace gram

#endif