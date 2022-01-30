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

namespace gram {
class Display;
class Grammar;
} // namespace gram

namespace gram {

enum ActionID : int;
enum StateID : int;
enum TransitionID : int;

struct Transition {
    mutable bool colored;
    ActionID action;
    StateID to; // Next state

    Transition(StateID _to, ActionID _action)
        : colored(true), action(_action), to(_to) {}

    bool operator<(Transition const &other) const noexcept {
        if (this->action != other.action)
            return this->action < other.action;
        return this->to < other.to;
    }
};

class Automaton;

struct TransitionSet {
    auto insert(const Transition &t) { return set.insert(t); }
    [[nodiscard]] auto size() const noexcept { return set.size(); }
    [[nodiscard]] auto begin() { return set.begin(); }
    [[nodiscard]] auto end() { return set.end(); }
    [[nodiscard]] auto begin() const { return set.begin(); }
    [[nodiscard]] auto end() const { return set.end(); }
    // Returns an iterator (possibly) pointering to the first element that has
    // the same action as the argument. We need to check by ourselves if the
    // iterator is valid (is still accessible and has the same action).
    [[nodiscard]] auto lowerBound(ActionID action) const {
        return set.lower_bound({StateID{-1}, action});
    }
    [[nodiscard]] bool containsAction(ActionID action) const {
        auto it = lowerBound(action);
        return it != set.end() && it->action == action;
    }
    TransitionSet() = default;

  private:
    std::set<Transition> set;
};

// Owns: nothing.
// For transformed DFA, state is actually a pseudo state.
class State {
  public:
    // This flag will be marked in Automaton::markFinalState().
    bool finalFlag = false;
    mutable bool colored = true;
    StateID id;
    const char *label = nullptr;
    TransitionSet *transitions = nullptr;
    util::BitSet<ActionID> *actionConstraints = nullptr;

  public:
    friend class Automaton;
    State(StateID id, const char *label, TransitionSet *trans,
          util::BitSet<ActionID> *constraints)
        : id(id), label(label), transitions(trans),
          actionConstraints(constraints) {}
};

using StateClosure = util::BitSet<StateID>;

} // namespace gram

namespace std {
template <> struct hash<gram::StateID> {
    static_assert(sizeof(gram::StateID) == sizeof(int),
                  "hash<int> is not suitable for StateID");
    std::size_t operator()(gram::StateID const &k) const {
        return hash<int>()(k);
    }
};
} // namespace std

namespace gram {

// Owns: transitions of states.
// Does not own: actions, label of states, constraints of states.
class Automaton {
  public:
    Automaton();
    Automaton(Automaton const &other) = delete;
    Automaton &operator=(Automaton const &other) = delete;
    Automaton(Automaton &&other) = default;
    Automaton &operator=(Automaton &&other) = default;

    // Add a new state. ID is ensured to grow continuously.
    StateID addState(const char *label, util::BitSet<ActionID> *constraints);
    ActionID addAction(const char *s);
    void addTransition(StateID from, StateID to, ActionID action);
    void addEpsilonTransition(StateID from, StateID to);
    void markStartState(StateID state);
    void markFinalState(StateID state);
    void setState(StateID state);
    void highlightState(StateID state) const;
    void setDumpFlag(bool flag);
    void setEpsilonAction(ActionID actionID);

    using ClosureEqualFuncType =
        std::function<bool(const StateClosure &, const StateClosure &)>;
    using DuplicateClosureHandlerType =
        std::function<void(StateClosure const &existingClosure,
                           StateClosure const &duplicateClosure)>;
    void setClosureEqualFunc(ClosureEqualFuncType const &func);
    void setDuplicateClosureHandler(DuplicateClosureHandlerType const &func);

    // Accessors
    [[nodiscard]] auto const &getAllStates() const { return states; }
    [[nodiscard]] StateID getState() const { return currentState; }
    [[nodiscard]] StateID getStartState() const { return startState; }
    [[nodiscard]] auto const &getAllActions() const { return actions; }
    [[nodiscard]] auto const &getClosures() const {
        assert(transformedDFAFlag);
        return closures;
    }
    [[nodiscard]] auto getFormerStates() const {
        assert(transformedDFAFlag);
        return formerStates;
    }
    [[nodiscard]] bool isEpsilonAction(ActionID action) const {
        return action == epsilonAction;
    }
    [[nodiscard]] bool isDFA() const { return !multiDestFlag; }

    // DFA generation
    void makeClosure(StateClosure &closure) const;

    [[nodiscard]] ::std::optional<StateClosure>
    transit(StateClosure const &closure, ActionID action) const;

    // Returns a new DFA which is transformed from this automaton.
    // Since the DFA is new, there is no need to call separateKernels().
    Automaton toDFA();

    // Dump in graphviz format.
    // `posMap` is used to switch state indexes so the result can be easier to
    // observe. It stores: {realState => stateAlias(label)}
    [[nodiscard]] String dump(const StateID *posMap = nullptr) const;
    // Dump State in human-readable string.
    [[nodiscard]] String dumpState(StateID stateID) const;
    // Dump StateClosure in human-readable string.
    // (StateClosure does not have essential information itself.)
    [[nodiscard]] String dumpStateClosure(StateClosure const &closure) const;

    // Try to accept a new action. Compared to unconditional setState(), move()
    // simulates step-by-step moving, and throws exception when action cannot
    // be final.
    // Up to 2022.1.25: Not used.
    void move(ActionID action);

    // Separate kernels of two automatons after copy assignment.
    // void separateKernels();

    // Clone entire automaton
    // [[nodiscard]] Automaton deepClone() const;

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
    std::vector<State> states;
    std::vector<const char *> actions;
//    std::unordered_map<StringView, ActionID> actIDMap;
    std::vector<std::unique_ptr<TransitionSet>> transitionPool;

    TransitionSet *newTransitionSet();

    // Passed down by parser
    ClosureEqualFuncType closureEqualFunc;
    DuplicateClosureHandlerType duplicateClosureHandler;

    // Following fields are for transformed DFA.
    // Any attempts to access those methods from an automaton not created
    // by toDFA() method will cause undefined behaviors.

    // For given action, determine which states can accept it.
    // This is used only in transformation to DFA and a transformed DFA.
    // And we won't calculate it until we know size of bitset.
    std::vector<util::BitSet<StateID>> actionReceivers;

    // Not empty only when this automaton is a DFA transformed from
    // a previous NFA. The bitmap is used to help trace original
    // states.
    std::vector<StateClosure> closures;

    // Not empty only when this automaton is a DFA transformed from
    // a previous DFA. The vector provides complete information about
    // former NFA states.
    std::vector<State> formerStates;
};
} // namespace gram

#endif