#ifndef LRPARSER_AUTOMATA_H
#define LRPARSER_AUTOMATA_H

#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/common.h"
#include "src/util/BitSet.h"
#include "src/util/Process.h"

namespace gram {
class Display;
class Grammar;
}  // namespace gram

namespace gram {

enum ActionID : int;
enum StateID : int;

struct StateIDPair {
    StateID from, to;
    StateIDPair(StateID a, StateID b) : from(a), to(b) {}
    bool operator==(StateIDPair const &other) const {
        return from == other.from && to == other.to;
    }
    bool operator<(StateIDPair const &other) const {
        if (from != other.from) return from < other.from;
        return to < other.to;
    }
};

// This error is thrown when current state is illegal. This may
// be caused by not setting start state.
class AutomatonIllegalStateError : public std::runtime_error {
   public:
    AutomatonIllegalStateError()
        : std::runtime_error("Automaton state is illegal") {}
};

class AutomatonUnacceptedActionError : public std::runtime_error {
   public:
    AutomatonUnacceptedActionError()
        : std::runtime_error("Action is not accepted by automaton") {}
};

struct Transition {
    StateID dest{-1};  // Next state
    ActionID action{-1};

    Transition(StateID to, ActionID action) : dest(to), action(action) {}

    bool operator==(Transition const &other) const {
        return action == other.action && dest == other.dest;
    }

    bool operator<(Transition const &other) const {
        if (action != other.action) return action < other.action;
        return dest < other.dest;
    }
};

class Automaton;

struct AutomatonOutputInfo {
    Automaton const *automaton;
    const char *outGroupTag;
};

}  // namespace gram

namespace std {
template <>
struct hash<gram::StateID> {
    std::size_t operator()(gram::StateID const &k) const {
        return hash<int>()(k);
    }
};

template <>
struct hash<gram::StateIDPair> {
    std::size_t operator()(gram::StateIDPair const &k) const {
        auto hasher = hash<int>();
        return hasher(k.from) * 31 + k.to;
    }
};
}  // namespace std

namespace gram {

struct State {
    bool acceptable;
    StateID id;

    std::unordered_multimap<ActionID, StateID> trans;
    String label;

    // Pass string by value so literals will not cause a twice-copying situation
    State(StateID id, String label, bool acceptable)
        : acceptable(acceptable), id(id), label(std::move(label)) {}
};

class Automaton {
   public:
    using DFAState = util::BitSet;

    Automaton();

    // Used in building
    StateID addState(String desc, bool acceptable = false);
    ActionID addAction(StringView desc);
    void addTransition(StateID from, StateID to, ActionID action);
    void addEpsilonTransition(StateID from, StateID to);
    void markStartState(StateID state);
    void markFinalState(StateID state);
    void setState(StateID state);

    // Used in DFA generation
    void toEpsClosure(DFAState &S) const;
    [[nodiscard]] auto transit(DFAState const &S, ActionID action) const
        -> ::std::optional<DFAState>;
    Automaton toDFA();

    // Access other information
    [[nodiscard]] auto getAllStates() const -> std::vector<State> const &;
    [[nodiscard]] StateID getState() const;
    [[nodiscard]] StateID getStartState() const;
    [[nodiscard]] auto getAllActions() const -> std::vector<String> const &;
    [[nodiscard]] auto getStatesBitmap() const -> std::vector<DFAState> const &;
    [[nodiscard]] auto getFormerStates() const -> std::vector<State> const &;
    [[nodiscard]] bool isEpsilonAction(ActionID action) const;
    [[nodiscard]] bool isDFA() const;

    // Dump in graphviz format.
    // `posMap` is used to switch state indexes so the result can be easier to
    // observe. It stores: {realState => stateAlias(label)}
    [[nodiscard]] String dump(const StateID *posMap = nullptr) const;

    void highlightState(StateID state) const;
    void highlightTransition(StateID from, StateID to) const;

    // Try to accept a new action. Returns false when action is not accepted.
    // The action shouldn't be an end-of-input. Compared to unconditional
    // setState(), move() simulates step-by-step moving.
    // Up to 2022.1.25: Not used
    void move(ActionID action);

   private:
    // Whenever a state has multiple transitions with the same action,
    // this flag is set. So if multiDestFlag is true, this automaton
    // is not a DFA.
    bool multiDestFlag = false;
    StateID startState{-1};
    StateID currentState{-1};
    ActionID epsilonAction{-1};
    std::vector<State> states;
    std::vector<String> actions;
    std::unordered_map<StringView, ActionID> actIDMap;

    // Used for color state in dump
    mutable std::set<StateID> stateHighlightSet;

    // Used for color transition in dump
    mutable std::set<StateIDPair> transitionHighlightSet;

    // --------------------- For transformed DFA  ---------------------
    // Any attempts to access those methods from an automaton not created
    // by toDFA() method will cause undefined behaviors.

    bool transformedDFAFlag = false;

    // For given action, determine which states can accept it.
    // This is used only in transformation to DFA and a transformed DFA.
    // And we won't calculate it until we know size of bitset.
    std::vector<DFAState> actionReceivers;

    // Not empty only when this automaton is a DFA transformed from
    // a previous NFA. The bitmap is used to help trace original
    // states.
    std::vector<DFAState> statesBitmap;

    // Not empty only when this automaton is a DFA transformed from
    // a previous DFA. The vector provides complete information about
    // former NFA states.
    std::vector<State> formerStates;
};
}  // namespace gram

#endif