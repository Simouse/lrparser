#ifndef LRPARSER_LR_H
#define LRPARSER_LR_H

#include <deque>
#include <istream>
#include <unordered_map>
#include <vector>

#include "src/automata/Automaton.h"
#include "src/grammar/Grammar.h"
#include "src/util/TokenReader.h"

namespace gram {

class Grammar;

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
    };

    // Store actionTable and gotoTable in the same place.
    // Unfortunately, C++ doesn't have dynamic high-dimensional arrays, so
    // we have to use std::vector instead.
    using ParseTable = ::std::vector<std::vector<std::vector<ParseAction>>>;

    explicit LRParser(const gram::Grammar &g) : gram(g) {}

    void buildNFA();
    void buildDFA();
    void buildParseTables();

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
    [[nodiscard]] String dumpParseTableEntry(StateID state,
                                             ActionID action) const;

    // Test given stream with parsed results
    bool test(::std::istream &stream);

   protected:
    bool inputFlag = true;
    StateID extendedEnd{-1};
    StateID extendedStart{-1};
    const gram::Grammar &gram;
    Automaton nfa;
    Automaton dfa;
    ParseTable parseTable;
    std::deque<SymbolID> InputQueue;
    std::vector<StateID> stateStack;
    std::vector<SymbolID> symbolStack;

    // This map stores states that can be reduced by certain productions
    std::unordered_map<StateID, ProductionID> reduceMap;

    // Subclasses should override this virtual function to provide checks.
    [[nodiscard]] virtual bool canAddParseTableEntry(
        StateID state, ActionID act, ParseAction pact) const = 0;

    // This function may not actually add the entry. It checks violations by
    // calling canAddParseTableEntry() first, and when it returns false, the
    // entry is discarded.
    void addParseTableEntry(StateID state, ActionID act, ParseAction pact);

    // Throws an error if reduction fails
    void reduce(ProductionID prodID);

   private:
    // Read next symbol from the given stream.
    // This is only used inside test() method.
    void readSymbol(util::TokenReader &reader);
};

}  // namespace gram

#endif