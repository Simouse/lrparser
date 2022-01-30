#ifndef LRPARSER_LR_H
#define LRPARSER_LR_H

#include <deque>
#include <functional>
#include <istream>
#include <unordered_map>
#include <vector>

#include "src/automata/Automaton.h"
#include "src/grammar/Grammar.h"
#include "src/util/TokenReader.h"

namespace gram {

class Grammar;

// struct StateSeed {
//     SymbolID symbolID;
//     std::shared_ptr<util::BitSet<ActionID>> actionConstraints;
//     std::vector<StateID> startState; // First states this seed generates
// };

// template<class StateSeedHash, class StateSeedEqual>
class LRParser {
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
    using ParserTable = ::std::vector<std::vector<std::set<ParseAction>>>;

    // Stores data to generate states from a symbol's productions
    using StateSeed =
        std::pair<SymbolID, std::shared_ptr<util::BitSet<ActionID>>>;

    explicit LRParser(const gram::Grammar &g) : gram(g) {}

    void buildNFA();
    void buildDFA();
    void buildParserTable();

    // Accessors
    [[nodiscard]] auto const &getParserTable() const { return parserTable; }
    [[nodiscard]] auto const &getGrammar() const { return gram; }
    [[nodiscard]] auto const &getNFA() const { return nfa; }
    [[nodiscard]] auto const &getDFA() const { return dfa; }
    [[nodiscard]] auto const &getStateStack() const { return stateStack; }
    [[nodiscard]] auto const &getInputQueue() const { return InputQueue; }
    [[nodiscard]] auto const &getSymbolStack() const { return symbolStack; }
    [[nodiscard]] bool hasMoreInput() const { return inputFlag; }

    // Format
    [[nodiscard]] String dumpParserTableEntry(StateID state,
                                             ActionID action) const;

    // Test given stream with parsed results
    bool test(::std::istream &stream);

  protected:
    bool inputFlag = true;
    // This flag should be configured in configure().
    // It will then be passed down to generated automaton.
    // [[configurable]]
    // bool stateDumpIncludeConstraints = false;
    StateID extendedEnd{-1};
    StateID extendedStart{-1};
    const gram::Grammar &gram;
    Automaton nfa;
    Automaton dfa;
    ParserTable parserTable;
    std::deque<SymbolID> InputQueue;
    std::vector<StateID> stateStack;
    std::vector<SymbolID> symbolStack;

    // This map stores states that can be reduced by certain productions
    std::unordered_map<StateID, ProductionID> reduceMap;

    // This function may not actually add the entry. It checks violations by
    // calling canAddParserTableEntry() first, and when it returns false, the
    // entry is discarded. When the entry is a reduce item, the former state
    // which causes the reduction (called `substate`) is passed.
    void addParserTableEntry(StateID state, ActionID act, ParseAction pact,
                            StateID substate = StateID{-1});

    // Throws an error if reduction fails.
    void reduce(ProductionID prodID);

    // Virtual functions.
    // These functions changes behaviors of the parser, and must be overrided.

    // This method checks if current entry is applicable to add to the table.
    [[nodiscard]] virtual bool
    canAddParserTableEntry(StateID state, ActionID act, ParseAction pact,
                          StateID substate = StateID{-1}) const = 0;

    // Decides if the chain of this seed is final.
    [[nodiscard]] virtual bool
    canMarkFinal(StateSeed const &seed, Production const &production) const = 0;

    [[nodiscard]] virtual bool shouldIncludeConstraintsInDump() const = 0;

    [[nodiscard]] virtual std::function<size_t(StateSeed const &seed)>
    getSeedHashFunc() const = 0;

    [[nodiscard]] virtual std::function<bool(StateSeed const &a,
                                             StateSeed const &b)>
    getSeedEqualFunc() const = 0;

  private:
    // Read next symbol from the given stream.
    // This is only used inside test() method.
    void readSymbol(util::TokenReader &reader);
};

} // namespace gram

#endif