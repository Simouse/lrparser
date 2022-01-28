#ifndef LRPARSER_LR1_H
#define LRPARSER_LR1_H

#include "src/grammar/Grammar.h"
#include "src/parser/LRParser.h"

namespace gram {
class LR1Parser : public LRParser {
   public:
    LR1Parser(Grammar const &g) : LRParser(g) {}

    // TODO:
    [[nodiscard]] bool canAddParseTableEntry(StateID state, ActionID act,
                                             ParseAction pact) const override {
        return true;
    }
};
}  // namespace gram

#endif