#ifndef LRPARSER_LR_H
#define LRPARSER_LR_H

#include <deque>
#include <functional>
#include <istream>
#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdlib>
#include <cstring>

#include "src/automata/Automaton.h"
#include "src/grammar/Grammar.h"
#include "src/util/BitSet.h"
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
    using StateSeed = std::pair<SymbolID, util::BitSet<ActionID> *>;

    explicit LRParser(const gram::Grammar &g) : gram(g) {}

    void buildNFA();
    void buildDFA();
    void buildParserTable();
    // Test given stream with parsed results
    bool test(::std::istream &stream);

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

  protected:
    bool inputFlag = true;
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
    // [productionID][index] ==> label
    std::vector<std::vector<const char *>> labelMap;
    std::vector<std::unique_ptr<util::BitSet<ActionID>>> constraintPool;
    std::vector<std::unique_ptr<char[]>> stringPool;

    // Virtual functions.
    // These functions change behaviors of the parser, and must be overridden.

    // This method checks if current entry is applicable to add to the table.
    [[nodiscard]] virtual bool
    canAddParserTableEntry(StateID state, ActionID act, ParseAction pact,
                           StateID subState) const = 0;

    // Decides if the chain of this seed is final.
    [[nodiscard]] virtual bool
    canMarkFinal(StateSeed const &seed, Production const &production) const = 0;

    [[nodiscard]] virtual bool shouldIncludeConstraintsInDump() const = 0;

    [[nodiscard]] virtual std::function<size_t(StateSeed const &seed)>
    getSeedHashFunc() const = 0;

    [[nodiscard]] virtual std::function<bool(StateSeed const &a,
                                             StateSeed const &b)>
    getSeedEqualFunc() const = 0;

    // These two functions are used by automaton, and have default values. Only
    // LALR will override them.
    [[nodiscard]] virtual Automaton::ClosureEqualFuncType
    getClosureEqualFunc() const;

    [[nodiscard]] virtual Automaton::DuplicateClosureHandlerType
    getDuplicateClosureHandler() const;

  private:
    // Read next symbol from the given stream.
    // This is only used inside test() method.
    void readSymbol(util::TokenReader &reader);

    // This function may not actually add the entry. It checks violations by
    // calling canAddParserTableEntry() first, and when it returns false, the
    // entry is discarded. When the entry is a reduction item, the former state
    // which causes the reduction (called `subState`) is passed.
    void addParserTableEntry(StateID state, ActionID act, ParseAction pact,
                             StateID subState = StateID{-1});

    // Throws an error if reduction fails.
    void reduce(ProductionID prodID);

    util::BitSet<ActionID> *newConstraint(size_t size) {
        constraintPool.push_back(
            std::make_unique<util::BitSet<ActionID>>(size));
        return constraintPool.back().get();
    }

    util::BitSet<ActionID> *
    newConstraint(util::BitSet<ActionID> &&constraints) {
        constraintPool.push_back(
            std::make_unique<util::BitSet<ActionID>>(std::move(constraints)));
        return constraintPool.back().get();
    }

    struct StrdupDeleter {
        void operator() (char **arg) {
            free(arg);
        }
    };

    // Copy a string, and stores it internally
    const char *newString(String const &s) {
        auto holder = std::make_unique<char[]>(s.size() + 1);
        char *buf = holder.get();
        strcpy(buf, s.c_str());
        stringPool.push_back(std::move(holder));
        return buf;
    }

    void buildNFAStateLabels();
};

} // namespace gram

#endif