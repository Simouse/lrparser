#ifndef LRPARSER_LR0_H
#define LRPARSER_LR0_H

#include "src/automata/PushDownAutomaton.h"
#include "src/grammar/Grammar.h"
#include "src/parser/LRParser.h"
#include <cstddef>
#include <functional>

namespace gram {
class LR0Parser : public LRParser {
  public:
    explicit LR0Parser(Grammar const &g) : LRParser(g) {}

  protected:
    [[nodiscard]] Constraint *
    resolveLocalConstraints(const Constraint *parentConstraint,
                            const Production &production,
                            int rhsIndex) override {
        return allTermConstraint;
    }
};
} // namespace gram

#endif