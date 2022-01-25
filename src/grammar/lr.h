#ifndef LRPARSER_ANALYSIS_H
#define LRPARSER_ANALYSIS_H

#include <istream>
#include <unordered_map>
#include <vector>

#include "src/automata/Automaton.h"
#include "src/grammar/Grammar.h"

namespace gram {
class Grammar;
}  // namespace gram

namespace gram {

// There should be a SyntaxAnalysis base class, but since
// I don't use that, it's unnecessary.
class SyntaxAnalysisLR {
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

        // Only for putting action into a set
        bool operator<(ParseAction const &other) const {
            if (type != other.type) return type < other.type;
            return untyped_data < other.untyped_data;
        }
    };

    // Store actionTable and gotoTable in the same place.
    // Unfortunately, C++ doesn't have dynamic high-dimensional arrays, so
    // we have to use std::vector instead.
    using ParseTable = std::vector<std::vector<std::vector<ParseAction>>>;

    explicit SyntaxAnalysisLR(const gram::Grammar &g) : gram(g) {}
    // For those analysis processes which don't need automatons,
    // those methods should throw exceptions.
    virtual void buildNFA() = 0;
    virtual void buildDFA() = 0;
    virtual void buildParseTables() = 0;
    [[nodiscard]] virtual ParseTable const &getParseTable() const = 0;
    [[nodiscard]] virtual Grammar const &getGrammar() const = 0;
    [[nodiscard]] virtual Automaton const &getDFA() const = 0;
    // `posMap` is used to switch state indexes for better observation.
    [[nodiscard]] virtual String dumpParseTableEntry(StateID state,
                                                     ActionID action) const = 0;
    // Test given stream with parsed results
    virtual bool test(std::istream &stream) = 0;

   protected:
    const gram::Grammar &gram;
};

class SyntaxAnalysisSLR : public SyntaxAnalysisLR {
   public:
    explicit SyntaxAnalysisSLR(const gram::Grammar &g) : SyntaxAnalysisLR(g) {}
    void buildNFA() override;
    void buildDFA() override;
    void buildParseTables() override;
    [[nodiscard]] ParseTable const &getParseTable() const override;
    [[nodiscard]] Grammar const &getGrammar() const override;
    [[nodiscard]] Automaton const &getDFA() const override;
    [[nodiscard]] String dumpParseTableEntry(StateID state,
                                             ActionID action) const override;
    // Test given stream with parsed results
    bool test(std::istream &stream) override;

   private:
    StateID extendedEnd{-1};
    StateID extendedStart{-1};
    Automaton nfa;
    Automaton dfa;
    ParseTable parseTable;
    std::vector<int> nextSymbolStack;
    std::vector<StateID> stateStack;
    std::vector<int> symbolStack;
    // This map stores states that can be reduced by certain productions
    std::unordered_map<StateID, ProductionID> reduceMap;
    void addParseTableEntry(StateID state, ActionID act, ParseAction pact);
    // Returns id of the symbol it can produce, or -1 if it's not possible
    void reduce(ProductionID prodID);
};

}  // namespace gram

#endif