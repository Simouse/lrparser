#ifndef __AUTOMATA_H__
#define __AUTOMATA_H__

#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/common.h"
#include "src/util/bitset.h"

namespace gram {
class Display;
class Grammar;
} // namespace gram

namespace gram {

// Transition must be used within Automaton.
// So an iterator should be enough.
// using ActionContainer = std::unordered_set<String>;
// using ActionIdentifier = ActionContainer::const_iterator;

enum ActionID : int;
enum StateID : int;

// Transition is not used as a storage type, but when constructing
// DFA, this structure is temporarily helpful.
struct Transition {
    StateID dest{-1}; // Next state
    ActionID action{-1};

    Transition(StateID to, ActionID action) : dest(to), action(action) {}

    bool operator==(Transition const &other) const {
        return action == other.action && dest == other.dest;
    }

    bool operator<(Transition const &other) const {
        if (action != other.action)
            return action < other.action;
        return dest < other.dest;
    }
};

} // namespace gram

namespace gram {

struct State {
    bool acceptable;
    StateID id;

    // Used only for LR syntax analysis. Stores state it can produce
    // with reducing process.
    // ActionID reduceAction{-1};

    std::unordered_multimap<ActionID, StateID> trans;
    String label;

    // Pass string by value so literals like "John" will not
    // cause a twice-copying situation
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

    // Used in LR syntax analysis. Set the action the reduce can produce.
    // TODO: rm setReduce() from automaton
    // void setReduce(StateID state, ActionID action);

    // Used in DFA generation
    void toEpsClosure(DFAState &S) const;
    [[nodiscard]] auto transit(DFAState const &S, ActionID action) const
        -> ::std::optional<DFAState>;
    Automaton toDFA();

    [[nodiscard]] auto getAllStates() const -> std::vector<State> const &;
    [[nodiscard]] int getState() const;
    [[nodiscard]] int getStartState() const;
    [[nodiscard]] auto getAllActions() const -> std::vector<String> const &;
    [[nodiscard]] auto getActionReceivers() const
        -> std::vector<DFAState> const &;
    [[nodiscard]] auto getStatesBitmap() const
        -> std::vector<DFAState> const &;
    [[nodiscard]] auto getFormerStates() const -> std::vector<State> const &;
    [[nodiscard]] bool isEpsilonAction(ActionID action) const;
    [[nodiscard]] bool isDFA() const;

    // Dump in graphviz format
    [[nodiscard]] String dump() const;

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

    // --------------------- For transformed DFA  ---------------------
    // Any attempts to access those methods from an automaton not created
    // by toDFA() method will cause undefined behaviors.

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

    // std::unordered_multimap<StateID, Transition> links;
};
} // namespace gram

#endif