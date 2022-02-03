#ifndef LRPARSER_SLR_H
#define LRPARSER_SLR_H

#include "src/grammar/Grammar.h"
#include "src/parser/LRParser.h"
namespace gram {
class SLRParser : public LRParser {
  public:
    explicit SLRParser(Grammar const &g) : LRParser(g) {}

  protected:
    [[nodiscard]] Constraint *
    resolveLocalConstraints(const Constraint *parentConstraint,
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