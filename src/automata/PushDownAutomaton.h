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
#include <string>

#include "src/common.h"
#include "src/util/BitSet.h"
#include "src/util/Process.h"
#include "src/util/ResourceProvider.h"

namespace gram {
class Grammar;

struct Transition {
    ActionID action;
    StateID destination; // Next state
    Transition(StateID to, ActionID action) : action(action), destination(to) {}
};

struct TransitionSet {
    // Returns a flag indicating whether the insertion makes the automaton a
    // NFA.
    auto insert(const Transition &key) {
        auto it = set.find(key);
        if (it == set.end() || it->destination != key.destination)
            return set.insert(key);
        return it;
    }
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

using Constraint = util::BitSet<ActionID>;
using Closure = util::BitSet<StateID>;

// For transformed DFA, state is a pseudo state, only containing transition
// information.
class State {
  public:
    ProductionID productionID; // Kernel production
    int rhsIndex;              // Kernel rhs index
    TransitionSet *transitions = nullptr;
    Constraint *constraint = nullptr;
    State(ProductionID prodID, int rhsIndex, TransitionSet *trans,
          Constraint *constraint)
        : productionID(prodID), rhsIndex(rhsIndex), transitions(trans),
          constraint(constraint) {}
};

} // namespace gram

namespace std {
template <> struct hash<StateID> {
    static_assert(sizeof(StateID) == sizeof(int),
                  "hash<int> is not suitable for StateID");
    std::size_t operator()(StateID const &k) const { return hash<int>()(k); }
};
} // namespace std

namespace gram {

// All pointer-form data does not belong to PushDownAutomaton, and
// PushDownAutomaton will not manage its storage. Before using its dump()
// method, all properties must be set properly by calling the methods following
// constructors.
class PushDownAutomaton {
  public:
    friend class LALRParser;

    explicit PushDownAutomaton(
        util::ResourceProvider<TransitionSet> *provider,
        std::vector<std::vector<const char *>> const *kernelLabelMap)
        : transitionSetProvider(provider), kernelLabelMap(kernelLabelMap) {}
    PushDownAutomaton(PushDownAutomaton const &other) = delete;
    PushDownAutomaton &operator=(PushDownAutomaton const &other) = delete;
    PushDownAutomaton(PushDownAutomaton &&other) = default;
    PushDownAutomaton &operator=(PushDownAutomaton &&other) = default;

    // Add a new state. ID is ensured to grow continuously.
    StateID addState(ProductionID prodID, int rhsIndex,
                     Constraint *constraints);
    StateID addPseudoState();
    ActionID addAction(const char *s);
    void addTransition(StateID from, StateID to, ActionID action);
    void addEpsilonTransition(StateID from, StateID to);
    void markStartState(StateID state);
    void highlightState(StateID state) const;
    void setDumpFlag(bool flag);
    void setEpsilonAction(ActionID actionID);
    void setEndOfInputAction(ActionID actionID);

    // Accessors
    [[nodiscard]] auto const &getAllStates() const { return states; }
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
    [[nodiscard]] bool isDFA() const { return !indefiniteFlag; }

    // DFA generation
    void makeClosure(Closure &closure) const;

    [[nodiscard]] std::optional<Closure>
    transit(Closure const &closure, ActionID action,
            std::vector<util::BitSet<StateID>> const &receiverVec) const;

    // Returns a new DFA which is transformed from this automaton.
    // Since the DFA is new, there is no need to call separateKernels().
    PushDownAutomaton toDFA();

    // Dump in graphviz format.
    // `posMap` is used to switch state indexes so the result can be easier to
    // observe. It stores: realState => stateAlias(label).
    [[nodiscard]] std::string dump() const;

    // Dump State in human-readable string. The second return value is a flag
    // indicating if this state can be final.
    [[nodiscard]] std::pair<std::string, bool> dumpState(StateID stateID) const;

    // Dump StateClosure in human-readable string. The second return value is a
    // flag indicating if this state can be final.
    [[nodiscard]] std::pair<std::string, bool>
    dumpClosure(Closure const &closure) const;

    // This error is thrown when current state is illegal. This may
    // be caused by not setting start state.
    class IllegalStateError : public std::runtime_error {
      public:
        IllegalStateError()
            : std::runtime_error("PushDownAutomaton state is illegal") {}
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
    // this flag is set. So if indefiniteFlag is true, this automaton
    // is not a DFA.
    bool indefiniteFlag = false;
    // To differentiate normal DFAs and transformed DFAs
    bool transformedDFAFlag = false;
    bool includeConstraints = false;
    StateID startState{-1};
    ActionID epsilonAction{-1};
    ActionID endOfInputAction{-1};
    util::ResourceProvider<TransitionSet> *transitionSetProvider;
    std::vector<std::vector<const char *>> const *kernelLabelMap;
    std::vector<State> states;
    std::vector<const char *> actions;
    // Contains highlight flags.
    mutable util::BitSet<StateID> highlightSet;
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