#ifndef LRPARSER_AUTOMATA_H
#define LRPARSER_AUTOMATA_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/common.h"
#include "src/util/BitSet.h"
#include "src/util/Process.h"
#include "src/util/ResourceProvider.h"

namespace gram {
class Display;
class Grammar;
} // namespace gram

namespace gram {

struct Transition {
    mutable bool colored;
    ActionID action;
    StateID destination; // Next state

    Transition(StateID to, ActionID action)
        : colored(true), action(action), destination(to) {}
};

class Automaton;

struct TransitionSet {
    auto insert(const Transition &t) { return set.insert(t); }
    [[nodiscard]] auto size() const noexcept { return set.size(); }
    [[nodiscard]] auto begin() { return set.begin(); }
    [[nodiscard]] auto end() { return set.end(); }
    [[nodiscard]] auto begin() const { return set.begin(); }
    [[nodiscard]] auto end() const { return set.end(); }
    [[nodiscard]] auto rangeOf(ActionID actionID) const {
        return set.equal_range(Transition{StateID{-1}, actionID});
    }
    [[nodiscard]] bool contains(ActionID action) const {
        auto range = rangeOf(action);
        return range.first != range.second;
    }
    TransitionSet() = default;

  private:
    struct TransitionComparator {
        bool operator()(const Transition &a, const Transition &b) const {
            return a.action < b.action;
        }
    };
    std::multiset<Transition, TransitionComparator> set;
};

// All pointer-form data does not belong to State, and State will not
// manage its storage.
// For transformed DFA, state is actually a pseudo state.
class State {
  private:
    using Constraint = util::BitSet<ActionID>;

  public:
    // This flag will be marked in Automaton::markFinalState().
    bool finalFlag = false;
    mutable bool colored = true;
    StateID id;
    // These two fields can be used to identity kernel.
    // `label` and `transitions` are not needed now.
    ProductionID productionID;
    int rhsIndex;
    const char *label = nullptr;
    TransitionSet *transitions = nullptr;
    Constraint *constraint = nullptr;

  public:
    friend class Automaton;
    State(StateID id, ProductionID prodID, int rhsIndex, const char *label,
          TransitionSet *trans, Constraint *constraint)
        : id(id), productionID(prodID), rhsIndex(rhsIndex), label(label),
          transitions(trans), constraint(constraint) {}
};

using Closure = util::BitSet<StateID>;

} // namespace gram

namespace std {
template <> struct hash<StateID> {
    static_assert(sizeof(StateID) == sizeof(int),
                  "hash<int> is not suitable for StateID");
    std::size_t operator()(StateID const &k) const { return hash<int>()(k); }
};
} // namespace std

namespace gram {

// All pointer-form data does not belong to Automaton, and Automaton will not
// manage its storage.
class Automaton {
  public:
    friend class LALRParser;

    Automaton(util::ResourceProvider<TransitionSet> *provider) {
        this->transitionSetProvider = provider;
    }
    Automaton(Automaton const &other) = delete;
    Automaton &operator=(Automaton const &other) = delete;
    Automaton(Automaton &&other) = default;
    Automaton &operator=(Automaton &&other) = default;

    // Add a new state. ID is ensured to grow continuously.
    StateID addState(ProductionID prodID, int rhsIndex, const char *label,
                     util::BitSet<ActionID> *constraints);
    StateID addPseudoState();
    ActionID addAction(const char *s);
    void addTransition(StateID from, StateID to, ActionID action);
    void addEpsilonTransition(StateID from, StateID to);
    void markStartState(StateID state);
    void markFinalState(StateID state);
    void setState(StateID state);
    void highlightState(StateID state) const;
    void setDumpFlag(bool flag);
    void setEpsilonAction(ActionID actionID);

    // using ClosureEqualFuncType =
    //     std::function<bool(const LALRClosure &, const LALRClosure &)>;
    // using DuplicateClosureHandlerType = std::function<void(
    //     LALRClosure const &existingClosure, LALRClosure const &duplicateClosure)>;
    // void setClosureEqualFunc(ClosureEqualFuncType const &func);
    // void setDuplicateClosureHandler(DuplicateClosureHandlerType const &func);

    // Accessors
    [[nodiscard]] auto const &getAllStates() const { return states; }
    [[nodiscard]] StateID getState() const { return currentState; }
    [[nodiscard]] StateID getStartState() const { return startState; }
    [[nodiscard]] auto const &getAllActions() const { return actions; }
    [[nodiscard]] auto const &getClosures() const {
        assert(transformedDFAFlag);
        return closures;
    }
    [[nodiscard]] auto getAuxStates() const {
        assert(transformedDFAFlag);
        return auxStates;
    }
    [[nodiscard]] bool isEpsilonAction(ActionID action) const {
        return action == epsilonAction;
    }
    [[nodiscard]] bool isDFA() const { return !multiDestFlag; }

    // DFA generation
    void makeClosure(Closure &closure) const;

    [[nodiscard]] ::std::optional<Closure>
    transit(Closure const &closure, ActionID action,
            std::vector<util::BitSet<StateID>> const &receiverVec) const;

    // Returns a new DFA which is transformed from this automaton.
    // Since the DFA is new, there is no need to call separateKernels().
    Automaton toDFA();

    // Dump in graphviz format.
    // `posMap` is used to switch state indexes so the result can be easier to
    // observe. It stores: {realState => stateAlias(label)}
    [[nodiscard]] std::string dump(const StateID *posMap = nullptr) const;
    // Dump State in human-readable string.
    [[nodiscard]] std::string dumpState(StateID stateID) const;
    // Dump StateClosure in human-readable string.
    // (StateClosure does not have essential information itself.)
    [[nodiscard]] std::string dumpClosure(Closure const &closure) const;

    // Try to accept a new action. Compared to unconditional setState(), move()
    // simulates step-by-step moving, and throws exception when action cannot
    // be final.
    // Up to 2022.1.25: Not used.
    void move(ActionID action);

    // This error is thrown when current state is illegal. This may
    // be caused by not setting start state.
    class IllegalStateError : public std::runtime_error {
      public:
        IllegalStateError()
            : std::runtime_error("Automaton state is illegal") {}
    };

    class UnacceptedActionError : public std::runtime_error {
      public:
        UnacceptedActionError()
            : std::runtime_error("Action is not accepted by automaton") {}
    };

    class AmbiguousDestinationError : public std::runtime_error {
      public:
        AmbiguousDestinationError()
            : std::runtime_error("Action is not accepted by automaton") {}
    };

  private:
    // Whenever a state has multiple transitions with the same action,
    // this flag is set. So if multiDestFlag is true, this automaton
    // is not a DFA.
    bool multiDestFlag = false;
    // To differentiate normal DFAs and transformed DFAs
    bool transformedDFAFlag = false;
    bool includeConstraints = false;
    StateID startState{-1};
    StateID currentState{-1};
    ActionID epsilonAction{-1};
    util::ResourceProvider<TransitionSet> *transitionSetProvider;
    std::vector<State> states;
    std::vector<const char *> actions;
    // std::unordered_map<StringView, ActionID> actIDMap;
    // std::vector<std::unique_ptr<TransitionSet>> transitionPool;

    // TransitionSet *newTransitionSet();

    // Passed down by parser
    // ClosureEqualFuncType closureEqualFunc;
    // DuplicateClosureHandlerType duplicateClosureHandler;

    // Following fields are for transformed DFA.
    // Any attempts to access those methods from an automaton not created
    // by toDFA() method will cause undefined behaviors.

    // Not empty only when this automaton is a DFA transformed from
    // a previous NFA. The bitmap is used to help trace original
    // states.
    std::vector<Closure> closures;

    // Not empty only when this automaton is a DFA transformed from
    // a previous DFA. The vector provides complete information about
    // former NFA states.
    std::vector<State> auxStates;
};
} // namespace gram

#endif