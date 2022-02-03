#ifndef LRPARSER_SLR_H
#define LRPARSER_SLR_H

#include "src/grammar/Grammar.h"
#include "src/parser/LRParser.h"
namespace gram {
class SLRParser : public LRParser {
  public:
    explicit SLRParser(Grammar const &g) : LRParser(g) {}

    [[nodiscard]] bool canAddParseTableEntry(StateID state, ActionID act,
                                             ParseAction pact,
                                             StateID substate) const override {
        if (pact.type == ParseAction::REDUCE) {
            auto const &production =
                gram.getProductionTable()[pact.productionID];
            auto const symbol = gram.getAllSymbols()[production.leftSymbol];
            return symbol.followSet.contains(static_cast<SymbolID>(act));
        }

        return true;
    }

    [[nodiscard]] bool
    canMarkFinal(const StateSeed &seed,
                 Production const &production) const override {
        auto endActionID = static_cast<ActionID>(gram.getEndOfInputSymbol().id);
        auto const &symbols = gram.getAllSymbols();
        return symbols[production.leftSymbol].followSet.contains(endActionID);
    }

    [[nodiscard]] bool shouldIncludeConstraintsInDump() const override {
        return false;
    }

    [[nodiscard]] util::BitSet<ActionID> *
    resolveLocalConstraints(const util::BitSet<ActionID> *parentConstraint,
                            const Production &production,
                            int rhsIndex) override {
        auto symbolID = production.rightSymbols[rhsIndex];
        auto const &symbols = gram.getAllSymbols();
        auto res = newConstraint(symbols[symbolID].followSet);
        // Ignore parentConstraint
        return res;
    }
};
} // namespace gram

#endif