#ifndef LRPARSER_AUTOMATA_H
#define LRPARSER_AUTOMATA_H

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

  private:
    std::set<Transition> set;
};

struct StateKernel {
    String label;
    TransitionSet transitionSet;
    explicit StateKernel(String lbl) : label(std::move(lbl)) {}
};

class State {
  public:
    // This flag will be marked in Automaton::markFinalState().
    bool endable;
    mutable bool colored;
    StateID id;
    std::shared_ptr<StateKernel> kernel;
    std::shared_ptr<util::BitSet<ActionID>> actionConstraints;

  public:
    friend class Automaton;
    State(StateID id, String label,
          std::shared_ptr<util::BitSet<ActionID>> constraints)
        : endable(false), colored(true), id(id),
          kernel(std::make_shared<StateKernel>(std::move(label))),
          actionConstraints(std::move(constraints)) {}

    [[nodiscard]] String &getLabel() const { return kernel->label; }

    [[nodiscard]] TransitionSet &getTransitionSet() const {
        return kernel->transitionSet;
    }
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

// To clone the entire automaton, use deepClone() instead.
class Automaton {
  public:
    Automaton();

    // Add a new state. ID is ensured to grow continuously.
    StateID addState(String desc,
                     std::shared_ptr<util::BitSet<ActionID>> constraints);
    ActionID addAction(StringView desc);
    void addTransition(StateID from, StateID to, ActionID action);
    void addEpsilonTransition(StateID from, StateID to);
    void markStartState(StateID state);
    void markFinalState(StateID state);
    void setState(StateID state);
    void highlightState(StateID state) const;
    void setDumpFlag(bool flag);

    // Accessors
    [[nodiscard]] auto getAllStates() const -> std::vector<State> const &;
    [[nodiscard]] StateID getState() const; // Current state
    [[nodiscard]] StateID getStartState() const;
    [[nodiscard]] auto getAllActions() const -> std::vector<String> const &;
    [[nodiscard]] auto getClosures() const -> std::vector<StateClosure> const &;
    [[nodiscard]] auto getFormerStates() const -> std::vector<State> const &;
    [[nodiscard]] bool isEpsilonAction(ActionID action) const;
    [[nodiscard]] bool isDFA() const;
    [[nodiscard]] auto getStatesMutable() -> std::vector<State> &;

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
    // TODO:
    [[nodiscard]] String dumpState(StateID stateID) const;

    // Dump StateClosure in human-readable string.
    // (StateClosure does not have essential information itself.)
    [[nodiscard]] String dumpStateClosure(StateClosure const &closure) const;

    // Try to accept a new action. Compared to unconditional setState(), move()
    // simulates step-by-step moving, and throws exception when action is not
    // endable.
    // Up to 2022.1.25: Not used.
    void move(ActionID action);

    // Separate kernels of two automatons after copy assignment.
    void separateKernels();

    // Clone entire automaton
    [[nodiscard]] Automaton deepClone() const;

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
    std::vector<String> actions;
    std::unordered_map<StringView, ActionID> actIDMap;

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