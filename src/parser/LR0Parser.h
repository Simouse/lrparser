#ifndef LRPARSER_LR0_H
#define LRPARSER_LR0_H

#include "src/automata/Automaton.h"
#include "src/grammar/Grammar.h"
#include "src/parser/LRParser.h"
#include <cstddef>
#include <functional>
namespace gram {
class LR0Parser : public LRParser {
  public:
    explicit LR0Parser(Grammar const &g) : LRParser(g) {}

    // No restriction
    [[nodiscard]] bool canAddParserTableEntry(StateID state, ActionID act,
                                             ParseAction pact,
                                             StateID substate) const override {
        return true;
    }

    [[nodiscard]] bool shouldIncludeConstraintsInDump() const override {
        return false;
    }

    [[nodiscard]] bool
    canMarkFinal(const StateSeed &seed,
                 Production const &production) const override {
        auto endActionID = static_cast<ActionID>(gram.getEndOfInputSymbol().id);
        auto const &symbols = gram.getAllSymbols();
        return symbols[production.leftSymbol].followSet.contains(endActionID);
    }

    [[nodiscard]] std::function<size_t(const StateSeed &)>
    getSeedHashFunc() const override {
        return [](StateSeed const &seed) -> std::size_t {
            return std::hash<int>()(seed.first);
        };
    }

    [[nodiscard]] std::function<bool(const StateSeed &, const StateSeed &)>
    getSeedEqualFunc() const override {
        return [](StateSeed const &s1, StateSeed const &s2) -> bool {
            return s1.first == s2.first;
        };
    }
};
} // namespace gram

#endif