#ifndef LRPARSER_SLR_H
#define LRPARSER_SLR_H

#include "src/grammar/Grammar.h"
#include "src/parser/LRParser.h"
namespace gram {
class SLRParser : public LRParser {
  public:
    SLRParser(Grammar const &g) : LRParser(g) {}

    [[nodiscard]] bool canAddParserTableEntry(StateID state, ActionID act,
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