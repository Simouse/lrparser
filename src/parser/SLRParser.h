#ifndef LRPARSER_SLR_H
#define LRPARSER_SLR_H

#include "src/grammar/Grammar.h"
#include "src/parser/LRParser.h"
namespace gram {
class SLRParser : public LRParser {
   public:
    SLRParser(Grammar const &g) : LRParser(g) {}

    [[nodiscard]] bool canAddParseTableEntry(StateID state, ActionID act,
                                             ParseAction pact) const override {
        if (pact.type == ParseAction::REDUCE) {
            auto const &production =
                gram.getProductionTable()[pact.productionID];
            auto const symbol = gram.getAllSymbols()[production.leftSymbol];
            return symbol.followSet.contains(static_cast<SymbolID>(act));
        }

        return true;
    }
    
};
}  // namespace gram

#endif