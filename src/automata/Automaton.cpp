#include "Automaton.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <optional>
#include <stack>
#include <unordered_map>
#include <utility>

#include "src/common.h"
#include "src/grammar/Grammar.h"
#include "src/util/BitSet.h"
#include "src/util/Formatter.h"
#include "src/util/Process.h"

namespace gram {

StateID Automaton::addState(ProductionID prodID, int rhsIndex,
                            const char *label,
                            util::BitSet<ActionID> *constraints) {
    auto id = static_cast<StateID>(states.size());
    states.emplace_back(id, prodID, rhsIndex, label,
                        transitionSetProvider->requestResource(), constraints);
    return id;
}

StateID Automaton::addPseudoState() {
    return addState(ProductionID{-1}, -1, nullptr, nullptr);
}

void Automaton::addTransition(StateID from, StateID to, ActionID action) {
    auto &trans = *states[from].transitions;

    if (trans.contains(action))
        this->multiDestFlag = true;

    trans.insert(Transition{to, action});
}

void Automaton::addEpsilonTransition(StateID from, StateID to) {
    assert(epsilonAction >= 0);
    return addTransition(from, to, epsilonAction);
}

void Automaton::highlightState(StateID state) const {
    states[state].colored = true;
}

void Automaton::setDumpFlag(bool flag) { includeConstraints = flag; }

void Automaton::markStartState(StateID state) {
    startState = state;
    currentState = state;

    highlightState(state);
}

void Automaton::setState(StateID state) { currentState = state; }

void Automaton::markFinalState(StateID state) {
    states[state].finalFlag = true;

    highlightState(state);
}

// Be cautious not to add duplicate actions
ActionID Automaton::addAction(const char *s) {
    auto actionID = static_cast<ActionID>(actions.size());
    actions.push_back(s);
    return actionID;
}

auto Automaton::dump(const StateID *posMap) const -> std::string {
    std::string s;
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

        std::string label = f.reverseEscaped(dumpState(state.id));
        s += f.formatView("  %d [label=\"%d: %s\"", mappedStateID,
                          mappedStateID, label.c_str());

        if (state.finalFlag)
            s += " peripheries=2";

        bool stateFillFlag = mappedStateID == getState();
        bool stateColorFlag = state.colored;
        state.colored = false;

        if (stateFillFlag) {
            s += R"( style="rounded,filled")";
            if (stateColorFlag)
                s += " color=red fontcolor=white";
        } else if (stateColorFlag)
            s += " color=red fontcolor=red";

        s += "]\n";
        if (mappedStateID == getStartState()) {
            s += f.formatView("  start -> %d\n", mappedStateID);
        }
        for (auto &tran : *state.transitions) {
            StateID mappedDestID =
                posMap ? posMap[tran.destination] : tran.destination;
            label = f.reverseEscaped(actions[tran.action]);
            s += f.formatView("  %d -> %d [label=\"%s\"", mappedStateID,
                              mappedDestID, label.c_str());
            if (isEpsilonAction(tran.action))
                s += " constraint=false";
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
void Automaton::makeClosure(Closure &closure) const {
    std::stack<StateID> stack;
    for (auto s : closure)
        stack.push(static_cast<StateID>(s));

    while (!stack.empty()) {
        auto s = stack.top();
        stack.pop();
        auto const &trans = *states[s].transitions;
        auto range = trans.rangeOf(epsilonAction);
        for (auto it = range.first; it != range.second; ++it) {
            // Can reach this state by epsilon
            if (!closure.contains(it->destination)) {
                closure.insert(it->destination);
                stack.push(it->destination);
            }
        }
    }
}

auto Automaton::transit(Closure const &closure, ActionID actionID,
                        std::vector<util::BitSet<StateID>> const &receiverVec)
    const -> ::std::optional<Closure> {
    assert(actionID != epsilonAction);

    bool found = false;
    Closure res{states.size()};

    // Copy bitset
    util::BitSet receivers = receiverVec[actionID];
    receivers &= closure;
    for (auto state : receivers) {
        // This state can receive current action
        auto const &trans = *states[state].transitions;
        auto range = trans.rangeOf(actionID);
        for (auto it = range.first; it != range.second; ++it) {
            res.insert(it->destination);
            found = true;
        }
    }

    if (!found)
        return {};

    makeClosure(res);
    return std::make_optional<Closure>(std::move(res));
}

// For automatons which is not generated by toDFA()
std::string Automaton::dumpState(StateID stateID) const {
    if (transformedDFAFlag) {
        return dumpClosure(closures[stateID]);
    }
    std::string s = states[stateID].label;
    auto const &constraints = states[stateID].constraint;
    if (includeConstraints && constraints) {
        s += ", ";
        bool slashFlag = false;
        for (auto actionID : *constraints) {
            if (slashFlag)
                s += '/';
            s += actions[actionID];
            slashFlag = true;
        }
    }
    return s;
}

std::string Automaton::dumpClosure(Closure const &closure) const {
    std::string s;
    bool newLineFlag = false;
    for (auto stateID : closure) {
        if (newLineFlag)
            s += '\n';
        s += auxStates[stateID].label;
        auto const &constraints = auxStates[stateID].constraint;
        if (includeConstraints && constraints) {
            s += ", ";
            bool slashFlag = false;
            for (auto actionID : *constraints) {
                if (slashFlag)
                    s += '/';
                s += actions[actionID];
                slashFlag = true;
            }
        }
        newLineFlag = true;
    }
    return s;
}

Automaton Automaton::toDFA() {
    Automaton dfa(this->transitionSetProvider);

    // Since our automaton can not be copyed easily, we must go through normal
    // constructing process even when automaton is a DFA.

    // Copy all actions to this new automaton. There are still some data
    // being built, they will be moved before this function returns.
    // NOTE: Epsilon action is no longer useful but is still copied,
    // so the index of other actions can stay the same.
    dfa.actions = this->actions;
    dfa.auxStates = this->states;
    dfa.transformedDFAFlag = true;
    dfa.setDumpFlag(includeConstraints);

    // The result is used by transit()
    std::vector<util::BitSet<StateID>> receiverVec(actions.size());
    for (auto &state : states) {
        for (auto &tran : *state.transitions)
            receiverVec[tran.action].insert(state.id);
    }

    // States that need to be processed.
    // We need to ensure that for S in `stack`, makeClosure(S) == S.
    std::stack<StateID> stack;
    std::unordered_map<Closure, StateID> closureIDMap;
    std::vector<Closure> closureVec;

    auto addNewState = [&closureIDMap, &closureVec, &dfa, &stack](Closure &&c) {
        auto stateIndex = static_cast<StateID>(closureVec.size());
        closureVec.push_back(std::move(c));
        closureIDMap.emplace(closureVec.back(), stateIndex);
        // Cannot decide label now. Constraints are of no use to minial DFA.
        dfa.addPseudoState();
        stack.push(stateIndex);
        return stateIndex;
    };

    // Add start state
    Closure start{states.size()};
    start.insert(startState);
    makeClosure(start);
    auto stateIndex = addNewState(std::move(start));
    dfa.markStartState(stateIndex);

    while (!stack.empty()) {
        auto stateID = stack.top();
        stack.pop();

        for (size_t i = 0; i < actions.size(); ++i) {
            auto actionID = static_cast<ActionID>(i);

            if (actionID == epsilonAction)
                continue;

            // If this action is not acceptable, the entire test will
            // be skipped.
            auto result = transit(closureVec[stateID], actionID, receiverVec);
            if (result.has_value()) {
                auto &value = result.value();
                auto existingIter = closureIDMap.find(value);

                if (existingIter == closureIDMap.end()) {
                    // Because LALR may combine constraints
                    auto nextStateID = addNewState(std::move(value));
                    // Add edge to this new state
                    dfa.addTransition(stateID, nextStateID, actionID);
                } else {
                    // Add edge to this previous state
                    dfa.addTransition(stateID, existingIter->second, actionID);
                }
            }
        }
    }

    // Mark final states
    // Step 1: Get a bitset of final NFA states
    util::BitSet<StateID> finalStateMask{states.size()};
    for (auto const &state : states) {
        // This property should be handled when building NFA
        if (state.finalFlag) {
            finalStateMask.insert(state.id);
        }
    }
    // Step 2: Use bitset to find DFA final states
    for (auto &entry : closureIDMap) {
        if (entry.first.hasIntersection(finalStateMask))
            dfa.markFinalState(entry.second);
    }

    // Copy former NFA states to DFA, so we can trace original states.
    dfa.closures = std::move(closureVec); // Move closures
    return dfa;
}

void Automaton::move(ActionID action) {
    if (currentState < 0)
        throw IllegalStateError();
    auto const &trans = *states[currentState].transitions;
    auto range = trans.rangeOf(action);
    if (range.first == trans.end()) {
        throw UnacceptedActionError();
    }
    StateID dest = range.first->destination;
    if (range.first != range.second && ++range.first != range.second) {
        throw AmbiguousDestinationError();
    }
    // Action is okay
    setState(dest);
}

void Automaton::setEpsilonAction(ActionID actionID) {
    epsilonAction = actionID;
}

} // namespace gram