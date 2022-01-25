#include "Automaton.h"

#include <vcruntime.h>

#include <algorithm>
#include <cassert>
#include <optional>
#include <stack>
#include <unordered_map>
#include <utility>

#include "src/common.h"
#include "src/grammar/Grammar.h"
#include "src/util/BitSet.h"
#include "src/util/Formatter.h"
#include "src/util/Process.h"

using gram::Grammar;
using std::unordered_map;

namespace gram {

// Do not add any initial states so when we copy grammar symbols into
// the automaton, the order is preserved: a symbol has an according
// automaton action.
Automaton::Automaton() = default;

StateID Automaton::addState(String desc, bool acceptable) {
    auto id = static_cast<StateID>(states.size());
    states.emplace_back(id, std::move(desc), acceptable);

    // Set highlight
    stateHighlightSet.insert(id);

    return id;
}

void Automaton::addTransition(StateID from, StateID to, ActionID action) {
    auto &trans = states[from].trans;
    auto it = trans.find(action);
    if (it == trans.end()) {
        // Add a new transition
        trans.emplace(action, to);
    } else if (it->second != to) {
        // The same action towards different states: set flag
        multiDestFlag = true;
        trans.emplace(action, to);
    }  // else: transition is already added: ignore

    // Set highlight
    transitionHighlightSet.emplace(from, to);
}

void Automaton::addEpsilonTransition(StateID from, StateID to) {
    if (epsilonAction < 0) {
        epsilonAction = addAction(Grammar::SignStrings::epsilon);
    }
    addTransition(from, to, epsilonAction);
}

void Automaton::markStartState(StateID state) {
    startState = state;
    currentState = state;

    highlightState(state);
}

void Automaton::setState(StateID state) { currentState = state; }

void Automaton::markFinalState(StateID state) {
    states[state].acceptable = true;

    highlightState(state);
}

std::vector<State> const &Automaton::getAllStates() const { return states; }

std::vector<String> const &Automaton::getAllActions() const { return actions; }

StateID Automaton::getState() const { return currentState; }

StateID Automaton::getStartState() const { return startState; }

auto Automaton::getStatesBitmap() const -> std::vector<DFAState> const & {
    assert(transformedDFAFlag);
    return statesBitmap;
}

auto Automaton::getFormerStates() const -> std::vector<State> const & {
    assert(transformedDFAFlag);
    return formerStates;
}

bool Automaton::isEpsilonAction(ActionID action) const {
    return action == epsilonAction;
}

bool Automaton::isDFA() const { return !multiDestFlag; }

ActionID Automaton::addAction(StringView desc) {
    auto it = actIDMap.find(desc);
    if (it != actIDMap.end()) return it->second;
    ActionID action{static_cast<int>(actions.size())};
    actions.emplace_back(desc.data(), desc.size());
    actIDMap.emplace(desc, action);
    return action;
}

auto Automaton::dump(const StateID *posMap) const -> String {
    String s;
    util::Formatter f;

    s.reserve(1024);

    s += "digraph G {\n"
         "  graph[center=true]\n"
         "  node [shape=box style=rounded]\n"
         "  edge[arrowsize=0.8 arrowhead=vee constraint=true]\n";

    // For NFA in our case, LR looks prettier
    if (!isDFA()) {
        s += "  rankdir=LR;\n";
    }

    // Add states
    if (getStartState() >= 0) {
        s += "  start [label=Start shape=plain]\n";
    }
    for (auto &state : states) {
        StateID mappedStateID = posMap ? posMap[state.id] : state.id;

        String label = f.reverseEscaped(state.label);
        s += f.formatView("  %d [label=\"%d: %s\"", mappedStateID,
                          mappedStateID, label.c_str());

        if (state.acceptable) s += " peripheries=2";

        bool stateFillFlag = mappedStateID == getState();
        bool stateHightFlag =
            stateHighlightSet.find(state.id) != stateHighlightSet.end();

        if (stateFillFlag) {
            s += R"( style="rounded,filled" fontname="times-bold")";
            if (stateHightFlag)
                s += R"( color=red fontcolor=white fontname="times-bold")";
        } else if (stateHightFlag)
            s += R"( color=red fontcolor=red fontname="times-bold")";

        s += "]\n";
        if (mappedStateID == getStartState()) {
            s += f.formatView("  start -> %d\n", mappedStateID);
        }
        for (auto &tran : state.trans) {
            StateID mappedDestID = posMap ? posMap[tran.second] : tran.second;
            label = f.reverseEscaped(actions[tran.first]);
            s += f.formatView("  %d -> %d [label=\"%s\"", mappedStateID,
                              mappedDestID, label.c_str());
            if (isEpsilonAction(tran.first)) s += " constraint=false";
            if (transitionHighlightSet.find({mappedStateID, mappedDestID}) !=
                transitionHighlightSet.end()) {
                s += " color=red fontcolor=red fontname=\"times-bold\"";
            }
            s += "]\n";
        }
    }

    s += "}";

    stateHighlightSet.clear();
    transitionHighlightSet.clear();

    return s;
}

void Automaton::highlightState(StateID state) const {
    stateHighlightSet.insert(state);
}

void Automaton::highlightTransition(StateID from, StateID to) const {
    transitionHighlightSet.emplace(from, to);
}

// This function calculate closure and store information in place;
// Must be used inside toDFA(), because only then transitions are sorted.
void Automaton::closurify(DFAState &dfaState) const {
    std::stack<StateID> stack;
    for (auto s : dfaState) stack.push(static_cast<StateID>(s));

    while (!stack.empty()) {
        auto s = stack.top();
        stack.pop();
        auto range = states[s].trans.equal_range(epsilonAction);
        for (auto it = range.first; it != range.second; ++it) {
            // Can reach this state by epsilon
            auto destination = it->second;
            if (!dfaState.contains(destination)) {
                dfaState.add(destination);
                stack.push(destination);
            }
        }
    }
}

// `action` here cannot be epsilon.
auto Automaton::transit(DFAState const &dfaState, ActionID action) const
    -> ::std::optional<DFAState> {
    assert(action != epsilonAction);

    bool found = false;
    DFAState res{states.size()};

    // Copy bitset
    DFAState receivers = actionReceivers[action];
    receivers &= dfaState;
    for (auto state : receivers) {
        // This state can receive current action
        auto range = states[state].trans.equal_range(action);
        for (auto it = range.first; it != range.second; ++it) {
            res.add(it->second);
            found = true;
        }
    }

    if (!found) return {};

    closurify(res);
    return std::make_optional<DFAState>(std::move(res));
}

void Automaton::buildActionReceivers() {
    actionReceivers.clear();
    actionReceivers.reserve(actions.size());
    for (size_t i = 0; i < actions.size(); ++i) {
        actionReceivers.emplace_back(states.size());
    }
    for (auto &state : states) {
        for (auto &tran : state.trans)
            actionReceivers[tran.first].add(state.id);
    }
}

Automaton Automaton::toDFA() {
    Automaton dfa;

    if (isDFA()) {
        dfa = *this; // Make a copy
        dfa.transformedDFAFlag = true;
        return dfa;
    }

    // Copy all actions to this new automaton.
    // Epsilon action is no longer useful but is still copyed,
    // so the index of other actions can stay the same.
    dfa.actions = this->actions;
    dfa.actIDMap = this->actIDMap;  // Maybe unnecessary

    // The result is used by transit()
    buildActionReceivers();

    // States that need to be processed.
    // We need to ensure that for S in `stack`, eps_closure(S) == S.
    std::stack<StateID> stack;
    std::unordered_map<DFAState, StateID> DFAStateIDMap;
    std::vector<DFAState> DFAStateVec;

    auto addNewState = [&DFAStateIDMap, &DFAStateVec, &dfa,
                        &stack](DFAState &&s) {
        auto stateIndex = static_cast<StateID>(DFAStateVec.size());
        DFAStateVec.push_back(std::move(s));
        DFAStateIDMap.emplace(DFAStateVec.back(), stateIndex);
        dfa.states.emplace_back(stateIndex, DFAStateVec.back().dump(), false);
        stack.push(stateIndex);
        return stateIndex;
    };

    // Add start state
    {
        DFAState start{states.size()};
        start.add(startState);
        closurify(start);

        auto stateIndex = addNewState(std::move(start));
        dfa.markStartState(stateIndex);
    }

    while (!stack.empty()) {
        auto stateID = stack.top();
        stack.pop();

        for (size_t actionIndex = 0; actionIndex < actions.size();
             ++actionIndex) {
            auto action = static_cast<ActionID>(actionIndex);

            if (action == epsilonAction) continue;

            // If this action is not acceptable, the entire test will
            // be skipped.
            auto result = transit(DFAStateVec[stateID], action);
            if (result.has_value()) {
                auto &value = result.value();
                auto existingIter = DFAStateIDMap.find(value);

                if (existingIter == DFAStateIDMap.end()) {
                    auto nextStateID = addNewState(std::move(value));
                    // Add edge to this new state
                    dfa.addTransition(stateID, nextStateID, action);
                } else {
                    // Add edge to this previous state
                    dfa.addTransition(stateID, existingIter->second, action);
                }
            }
        }
    }

    // Mark final states
    // Step 1: Get a bitset of final NFA states
    DFAState nfaFinalStates{states.size()};
    for (auto const &state : states) {
        if (state.acceptable) nfaFinalStates.add(state.id);
    }
    // Step 2: Use final NFA states to find NFA final states
    for (auto &entry : DFAStateIDMap) {
        if (entry.first.hasIntersection(nfaFinalStates))
            dfa.markFinalState(entry.second);
    }

    // Copy former NFA states to DFA, so we can trace original states.
    dfa.statesBitmap = std::move(DFAStateVec);  // Move bitmaps
    dfa.formerStates = this->states;            // Copy vec
    // Can I use a moved vector again?
    // https://stackoverflow.com/a/9168917/13785815
    // Yes, but after calling clear()
    dfa.actionReceivers = std::move(actionReceivers);
    dfa.transformedDFAFlag = true;
    return dfa;
}

void Automaton::move(ActionID action) {
    if (currentState < 0) throw AutomatonIllegalStateError();
    auto const &trans = states[currentState].trans;
    auto it = trans.find(action);
    if (it == trans.end()) throw AutomatonUnacceptedActionError();
    // Action is okay
    setState(it->second);
}

}  // namespace gram