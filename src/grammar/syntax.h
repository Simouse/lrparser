#ifndef __ANALYSIS_H__
#define __ANALYSIS_H__

#include "src/automata/automata.h"
#include "src/grammar/grammar.h"
#include <unordered_map>
#include <vector>

namespace gram {
class Grammar;
class Display;
} // namespace gram

namespace gram {
class SyntaxAnalysisLR {
  public:
    struct ParseAction {
        enum Type { GOTO, SHIFT, REDUCE, SUCCESS };
        Type type;
        union {
            StateID dest;
            ActionID action;
            int untyped;
        };
        ParseAction(Type t, int data) : type(t), untyped(data) {}

        // Only for putting action into a set
        bool operator<(ParseAction const &other) const {
            if (type != other.type)
                return type < other.type;
            return untyped < other.untyped;
        }
    };

    // Store actionTable and gotoTable in the same place
    // using ParseTable = std::unordered_map<
    //     StateID, std::unordered_map<ActionID, std::vector<ParseAction>>>;
    using ParseTable = std::vector<std::vector<std::vector<ParseAction>>>;

    SyntaxAnalysisLR(const gram::Grammar &g) : gram(g) {}
    // For those analysis processes which don't need automatons,
    // those methods should throw exceptions.
    virtual void buildNFA() = 0;
    virtual void buildDFA() = 0;
    virtual void buildParseTables() = 0;
    [[nodiscard]] virtual ParseTable const &getActionTable() const = 0;
    [[nodiscard]] virtual Grammar const &getGrammer() const = 0;
    [[nodiscard]] virtual Automaton const &getDFA() const = 0;
    [[nodiscard]] virtual String dumpParseTableEntry(StateID state,
                                                     ActionID action) const = 0;
    // [[nodiscard]] virtual StateID getExtEnd() const = 0;

  protected:
    const gram::Grammar &gram;
};

class SyntaxAnalysisSLR : public SyntaxAnalysisLR {
  public:
    SyntaxAnalysisSLR(const gram::Grammar &g) : SyntaxAnalysisLR(g) {}
    void buildNFA() override;
    void buildDFA() override;
    void buildParseTables() override;
    [[nodiscard]] ParseTable const &getActionTable() const override;
    [[nodiscard]] Grammar const &getGrammer() const override;
    [[nodiscard]] Automaton const &getDFA() const override;
    [[nodiscard]] String dumpParseTableEntry(StateID state,
                                             ActionID action) const override;
    // [[nodiscard]] StateID getExtEnd() const override;

  private:
    StateID extendedEnd{-1};
    Automaton nfa;
    Automaton dfa;
    ParseTable actionTable;
    // This map stores states that can reduce by certain productions
    std::unordered_map<StateID, ProductionID> reduceMap;
    void addParseTableEntry(StateID state, ActionID act, ParseAction pact);
};

} // namespace gram

#endif