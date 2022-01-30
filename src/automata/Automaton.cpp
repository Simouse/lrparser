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

using gram::Grammar;
using std::unordered_map;

namespace gram {

// Do not add any initial states so when we copy grammar symbols into
// the automaton, the order is preserved: a symbol has an according
// automaton action.
Automaton::Automaton() = default;

StateID Automaton::addState(const char *label, util::BitSet<ActionID> *constraints) {
    auto id = static_cast<StateID>(states.size());
    states.emplace_back(id, label, newTransitionSet(), constraints);
    return id;
}

void Automaton::addTransition(StateID from, StateID to, ActionID action) {
    auto &trans = *states[from].transitions;

    if (trans.containsAction(action))
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

void Automaton::setClosureEqualFunc(ClosureEqualFuncType const &func) {
    this->closureEqualFunc = func;
}

void Automaton::setDuplicateClosureHandler(
    const DuplicateClosureHandlerType &func) {
    this->duplicateClosureHandler = func;
}

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
//    StringView view = s;
//    auto it = actIDMap.find(view);
//    if (it != actIDMap.end())
//        return it->second;
    auto actionID = static_cast<ActionID>(actions.size());
//    actIDMap.emplace(view, actionID);
    actions.push_back(s);
    return actionID;
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

        String label = f.reverseEscaped(dumpState(state.id));
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
            StateID mappedDestID = posMap ? posMap[tran.to] : tran.to;
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
void Automaton::makeClosure(StateClosure &closure) const {
    std::stack<StateID> stack;
    for (auto s : closure)
        stack.push(static_cast<StateID>(s));

    while (!stack.empty()) {
        auto s = stack.top();
        stack.pop();
        auto const &trans = *states[s].transitions;
        auto it = trans.lowerBound(epsilonAction);
        auto end = trans.end();
        for (; it != end && it->action == epsilonAction; ++it) {
            // Can reach this state by epsilon
            if (!closure.contains(it->to)) {
                closure.insert(it->to);
                stack.push(it->to);
            }
        }
    }
}

// `action` here cannot be epsilon.
auto Automaton::transit(StateClosure const &closure, ActionID actionID) const
    -> ::std::optional<StateClosure> {
    assert(actionID != epsilonAction);

    bool found = false;
    StateClosure res{states.size()};

    // Copy bitset
    util::BitSet receivers = actionReceivers[actionID];
    receivers &= closure;
    for (auto state : receivers) {
        // This state can receive current action
        auto const &trans = *states[state].transitions;
        auto it = trans.lowerBound(actionID);
        auto end = trans.end();
        for (; it != end && it->action == actionID; ++it) {
            res.insert(it->to);
            found = true;
        }
    }

    if (!found)
        return {};

    makeClosure(res);
    return std::make_optional<StateClosure>(std::move(res));
}

// For automatons which is not generated by toDFA()
String Automaton::dumpState(StateID stateID) const {
    if (transformedDFAFlag) {
        return dumpStateClosure(closures[stateID]);
    }
    String s = states[stateID].label;
    auto const &constraints = states[stateID].actionConstraints;
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

String Automaton::dumpStateClosure(StateClosure const &closure) const {
    String s;
    bool newLineFlag = false;
    for (auto stateID : closure) {
        if (newLineFlag)
            s += '\n';
        s += formerStates[stateID].label;
        auto const &constraints = formerStates[stateID].actionConstraints;
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

TransitionSet *Automaton::newTransitionSet() {
    transitionPool.push_back(std::make_unique<TransitionSet>());
    return transitionPool.back().get();
}

Automaton Automaton::toDFA() {
    Automaton dfa;

    // Since our automaton can not be copyed easily, we must go through normal
    // constructing process even when automaton is a DFA.

    // if (isDFA()) {
    //     dfa = *this;
    //     dfa.separateKernels();
    //     dfa.transformedDFAFlag = true;
    //     return dfa;
    // }

    // Copy all actions to this new automaton. There are still some data
    // being built, they will be moved before this function returns.
    // NOTE: Epsilon action is no longer useful but is still copied,
    // so the index of other actions can stay the same.
    dfa.actions = this->actions;
//    dfa.actIDMap = this->actIDMap;
    dfa.formerStates = this->states;
    dfa.transformedDFAFlag = true;
    dfa.setDumpFlag(includeConstraints);

    // The result is used by transit()
    actionReceivers.clear();
    actionReceivers.reserve(actions.size());
    for (size_t i = 0; i < actions.size(); ++i) {
        actionReceivers.emplace_back(states.size());
    }
    for (auto &state : states) {
        for (auto &tran : *state.transitions)
            actionReceivers[tran.action].insert(state.id);
    }

    auto estimatedSize = 36;

    // States that need to be processed.
    // We need to ensure that for S in `stack`, makeClosure(S) == S.
    std::stack<StateID> stack;
    std::unordered_map<StateClosure, StateID, std::hash<StateClosure>,
                       decltype(closureEqualFunc)>
        closureIDMap(estimatedSize, std::hash<StateClosure>(),
                     closureEqualFunc);
    std::vector<StateClosure> closureVec;

    auto addNewState = [&closureIDMap, &closureVec, &dfa,
                        &stack](StateClosure &&sc) {
        auto stateIndex = static_cast<StateID>(closureVec.size());
        closureVec.push_back(std::move(sc));
        closureIDMap.emplace(closureVec.back(), stateIndex);
        // Cannot decide label now. Constraints are of no use to minial DFA.
        dfa.addState(nullptr, nullptr);
        stack.push(stateIndex);
        return stateIndex;
    };

    // Add start state
    StateClosure start{states.size()};
    start.insert(startState);
    makeClosure(start);
    auto stateIndex = addNewState(std::move(start));
    dfa.markStartState(stateIndex);

    while (!stack.empty()) {
        auto stateID = stack.top();
        stack.pop();

        for (size_t actionIndex = 0; actionIndex < actions.size();
             ++actionIndex) {
            auto action = static_cast<ActionID>(actionIndex);

            if (action == epsilonAction)
                continue;

            // If this action is not acceptable, the entire test will
            // be skipped.
            auto result = transit(closureVec[stateID], action);
            if (result.has_value()) {
                auto &value = result.value();
                auto existingIter = closureIDMap.find(value);

                if (existingIter == closureIDMap.end()) {
                    // Because LALR may combine constraints
                    auto nextStateID = addNewState(std::move(value));
                    // Add edge to this new state
                    dfa.addTransition(stateID, nextStateID, action);
                } else {
                    // Add edge to this previous state
                    dfa.addTransition(stateID, existingIter->second, action);
                    // Update previous constraints (for LALR)
                    duplicateClosureHandler(existingIter->first, value);
                }
            }
        }
    }

    // Mark final states
    // Step 1: Get a bitset of final NFA states
    util::BitSet<StateID> finalStateMask{states.size()};
    for (auto const &state : states) {
        if (state.finalFlag) // This property should be handled when building NFA
            finalStateMask.insert(state.id);
    }
    // Step 2: Use bitset to find DFA final states
    for (auto &entry : closureIDMap) {
        if (entry.first.hasIntersection(finalStateMask))
            dfa.markFinalState(entry.second);
    }

    // Copy former NFA states to DFA, so we can trace original states.
    dfa.closures = std::move(closureVec); // Move closures
    // Can I use a moved vector again?
    // https://stackoverflow.com/a/9168917/13785815
    // Yes, but after calling clear()
    dfa.actionReceivers = std::move(actionReceivers);
    return dfa;
}

void Automaton::move(ActionID action) {
    if (currentState < 0)
        throw IllegalStateError();
    auto const &trans = *states[currentState].transitions;
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

void Automaton::setEpsilonAction(ActionID actionID) {
    epsilonAction = actionID;
}

// void Automaton::separateKernels() {
//     for (auto &state : this->states) {
//         if (state.kernel)
//             state.kernel = std::make_shared<StateKernel>(*state.kernel);
//     }
//     for (auto &state : this->formerStates) {
//         if (state.kernel)
//             state.kernel = std::make_shared<StateKernel>(*state.kernel);
//     }
// }

// Automaton Automaton::deepClone() const {
//     Automaton res = *this;
//     res.separateKernels();
//     return res;
// }

} // namespace gram