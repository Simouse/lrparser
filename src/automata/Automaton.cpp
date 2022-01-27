#include "Automaton.h"

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
    return id;
}

void Automaton::addTransition(StateID from, StateID to, ActionID action) {
    auto &tranSet = states[from].tranSet;

    if (tranSet.containsAction(action)) {
        this->multiDestFlag = true;
    }

    tranSet.insert(Transition{to, action});
}

void Automaton::addEpsilonTransition(StateID from, StateID to) {
    if (epsilonAction < 0) {
        epsilonAction = addAction(Grammar::SignStrings::epsilon);
    }
    return addTransition(from, to, epsilonAction);
}

void Automaton::highlightState(StateID state) const {
    states[state].colored = true;
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

auto Automaton::getClosures() const -> std::vector<StateClosure> const & {
    assert(transformedDFAFlag);
    return closures;
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
    for (auto const &state : states) {
        StateID mappedStateID = posMap ? posMap[state.id] : state.id;

        String label = f.reverseEscaped(state.label);
        s += f.formatView("  %d [label=\"%d: %s\"", mappedStateID,
                          mappedStateID, label.c_str());

        if (state.acceptable) s += " peripheries=2";

        bool stateFillFlag = mappedStateID == getState();
        bool stateColorFlag = state.colored;
        state.colored = false;

        if (stateFillFlag) {
            s += R"( style="rounded,filled")";
            if (stateColorFlag) s += " color=red fontcolor=white";
        } else if (stateColorFlag)
            s += " color=red fontcolor=red";

        s += "]\n";
        if (mappedStateID == getStartState()) {
            s += f.formatView("  start -> %d\n", mappedStateID);
        }
        for (auto &tran : state.tranSet) {
            StateID mappedDestID = posMap ? posMap[tran.to] : tran.to;
            label = f.reverseEscaped(actions[tran.action]);
            s += f.formatView("  %d -> %d [label=\"%s\"", mappedStateID,
                              mappedDestID, label.c_str());
            if (isEpsilonAction(tran.action)) s += " constraint=false";
            if (tran.colored) {
                s += " color=red fontcolor=red fontname=\"times-bold\"";
                tran.colored = false;
            }
            s += "]\n";
        }
    }

    s += "}";

    return s;
}

// This function calculate closure and store information in place;
// Must be used inside toDFA(), because only then transitions are sorted.
void Automaton::closurify(StateClosure &closure) const {
    std::stack<StateID> stack;
    for (auto s : closure.states) stack.push(static_cast<StateID>(s));

    while (!stack.empty()) {
        auto s = stack.top();
        stack.pop();
        auto it = states[s].tranSet.lowerBound(epsilonAction);
        auto end = states[s].tranSet.end();
        for (; it != end && it->action == epsilonAction; ++it) {
            // Can reach this state by epsilon
            if (!closure.states.contains(it->to)) {
                closure.states.add(it->to);
                stack.push(it->to);
            }
        }
    }
}

// `action` here cannot be epsilon.
auto Automaton::transit(StateClosure const &closure, ActionID action) const
    -> ::std::optional<StateClosure> {
    assert(action != epsilonAction);

    bool found = false;
    StateClosure res{util::BitSet{states.size()}, closure.actionConstraints};

    // Copy bitset
    util::BitSet receivers = actionReceivers[action];
    receivers &= closure.states;
    for (auto state : receivers) {
        // This state can receive current action
        auto it = states[state].tranSet.lowerBound(action);
        auto end = states[state].tranSet.end();
        for (; it != end && it->action == action; ++it) {
            res.states.add(it->to);
            found = true;
        }
    }

    if (!found) return {};

    closurify(res);
    return std::make_optional<StateClosure>(std::move(res));
}

String Automaton::dumpStateClosure(StateClosure const &closure) const {
    String s = closure.states.dump();
    if (closure.actionConstraints.has_value()) {
        auto const &constraints = closure.actionConstraints.value();
        bool commaFlag = false;
        s += ", {";
        for (auto i : constraints) {
            if (commaFlag) s += ',';
            s += actions[i];
            commaFlag = true;
        }
        s += "}";
    }
    return s;
}

Automaton Automaton::toDFA() {
    Automaton dfa;

    // Make a copy if this is already a DFA
    if (isDFA()) {
        dfa = *this;
        dfa.transformedDFAFlag = true;
        return dfa;
    }

    // Copy all actions to this new automaton.
    // NOTE: Epsilon action is no longer useful but is still copyed,
    // so the index of other actions can stay the same.
    dfa.actions = this->actions;
    dfa.actIDMap = this->actIDMap;

    // The result is used by transit()
    actionReceivers.clear();
    actionReceivers.reserve(actions.size());
    for (size_t i = 0; i < actions.size(); ++i) {
        actionReceivers.emplace_back(states.size());
    }
    for (auto &state : states) {
        for (auto &tran : state.tranSet)
            actionReceivers[tran.action].add(state.id);
    }

    // States that need to be processed.
    // We need to ensure that for S in `stack`, closurify(S) == S.
    std::stack<StateID> stack;
    std::unordered_map<StateClosure, StateID> closureIDMap;
    std::vector<StateClosure> closureVec;

    auto addNewState = [&closureIDMap, &closureVec, &dfa,
                        &stack](StateClosure &&s) {
        auto stateIndex = static_cast<StateID>(closureVec.size());
        closureVec.push_back(std::move(s));
        closureIDMap.emplace(closureVec.back(), stateIndex);
        dfa.states.emplace_back(stateIndex,
                                dfa.dumpStateClosure(closureVec.back()), false);
        stack.push(stateIndex);
        return stateIndex;
    };

    // Add start state
    {
        StateClosure start{util::BitSet{states.size()}, std::nullopt};
        start.states.add(startState);
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
            auto result = transit(closureVec[stateID], action);
            if (result.has_value()) {
                auto &value = result.value();
                auto existingIter = closureIDMap.find(value);

                if (existingIter == closureIDMap.end()) {
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
    util::BitSet nfaFinalStates{states.size()};
    for (auto const &state : states) {
        if (state.acceptable) nfaFinalStates.add(state.id);
    }
    // Step 2: Use bitset to find DFA final states
    for (auto &entry : closureIDMap) {
        if (entry.first.states.hasIntersection(nfaFinalStates))
            dfa.markFinalState(entry.second);
    }

    // Copy former NFA states to DFA, so we can trace original states.
    dfa.closures = std::move(closureVec);  // Move closures
    dfa.formerStates = this->states;       // Copy states
    // Can I use a moved vector again?
    // https://stackoverflow.com/a/9168917/13785815
    // Yes, but after calling clear()
    dfa.actionReceivers = std::move(actionReceivers);
    dfa.transformedDFAFlag = true;
    return dfa;
}

void Automaton::move(ActionID action) {
    if (currentState < 0) throw IllegalStateError();
    auto const &trans = states[currentState].tranSet;
    auto it = trans.lowerBound(action);
    auto end = trans.end();
    if (it == end || it->action != action) {
        throw UnacceptedActionError();
    }
    StateID dest = it->to;
    if (++it != end && it->action == action) {
        throw AmbiguousDestinationError();
    }
    // Action is okay
    setState(dest);
}

}  // namespace gram