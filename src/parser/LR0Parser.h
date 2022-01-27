#ifndef LRPARSER_LR0_H
#define LRPARSER_LR0_H

#include "src/grammar/Grammar.h"
#include "src/parser/LRParser.h"
namespace gram {
class LR0Parser : public LRParser {
   public:
    LR0Parser(Grammar const &g) : LRParser(g) {}

    // No restriction
    [[nodiscard]] bool canAddParseTableEntry(StateID state, ActionID act,
                                             ParseAction pact) const override {
        return true;
    }
};
}  // namespace gram

#endif