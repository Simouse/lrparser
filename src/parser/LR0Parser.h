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
    [[nodiscard]] bool canAddParseTableEntry(StateID state, ActionID act,
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

    [[nodiscard]] util::BitSet<ActionID> *
    resolveLocalConstraints(const util::BitSet<ActionID> *parentConstraint,
                            const Production &production,
                            int rhsIndex) override {
        return allTermConstraint;
    }
};
} // namespace gram

#endif