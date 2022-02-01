#ifndef LRPARSER_LALR_H
#define LRPARSER_LALR_H

#include "src/common.h"
#include "src/grammar/Grammar.h"
#include "src/parser/LRParser.h"
namespace gram {
class LALRParser : public LRParser {
  public:
    explicit LALRParser(Grammar const &g) : LRParser(g) {}

    [[nodiscard]] bool canAddParserTableEntry(StateID state, ActionID act,
                                              ParseAction pact,
                                              StateID substate) const override {
        // We need to check specific reduction rule
        if (pact.type == ParseAction::REDUCE) {
            auto const &formerStates = dfa.getFormerStates();
            auto const &reducibleState = formerStates[substate];
            return reducibleState.constraint->contains(act);
        }

        return true;
    }

    [[nodiscard]] bool
    canMarkFinal(const StateSeed &seed,
                 Production const &production) const override {
        auto endActionID = static_cast<ActionID>(gram.getEndOfInputSymbol().id);
        assert(seed.second);
        return seed.second->contains(endActionID);
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

    // Only consider kernel
    [[nodiscard]] Automaton::ClosureEqualFuncType
    getClosureEqualFunc() const override {
        throw UnimplementedError();
    }

    // Handler: void(first arg: existingClosure, second arg: duplicateClosure)
    [[nodiscard]] Automaton::DuplicateClosureHandlerType
    getDuplicateClosureHandler() const override {
        throw UnimplementedError();
    }
};
} // namespace gram

#endif