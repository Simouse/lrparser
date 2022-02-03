#include "PushDownAutomaton.h"

#include <cassert>
#include <optional>
#include <stack>
#include <unordered_map>
#include <utility>

#include "src/common.h"
#include "src/util/BitSet.h"
#include "src/util/Formatter.h"

namespace gram {

StateID PushDownAutomaton::addState(ProductionID prodID, int rhsIndex,
                                    Constraint *constraints) {
    auto id = static_cast<StateID>(states.size());
    states.emplace_back(prodID, rhsIndex,
                        transitionSetProvider->requestResource(), constraints);
    highlightState(id);
    return id;
}

StateID PushDownAutomaton::addPseudoState() {
    return addState(ProductionID{-1}, -1, nullptr);
}

void PushDownAutomaton::addTransition(StateID from, StateID to,
                                      ActionID action) {
    auto &trans = *states[from].transitions;
    trans.insert(Transition{to, action});
}

void PushDownAutomaton::addEpsilonTransition(StateID from, StateID to) {
    assert(epsilonAction >= 0);
    return addTransition(from, to, epsilonAction);
}

void PushDownAutomaton::highlightState(StateID state) const {
    highlightSet.insert(state);
}

void PushDownAutomaton::setDumpFlag(bool flag) { includeConstraints = flag; }

void PushDownAutomaton::markStartState(StateID state) {
    startState = state;
    highlightState(state);
}

// Be cautious not to add duplicate actions
ActionID PushDownAutomaton::addAction(const char *s) {
    auto actionID = static_cast<ActionID>(actions.size());
    actions.push_back(s);
    return actionID;
}

auto PushDownAutomaton::dump() const -> std::string {
    std::string s;
    util::Formatter f;

    s.reserve(1024);

    s += "digraph G {\n"
         "  graph[center=true]\n"
         "  node [shape=box style=rounded]\n"
         "  edge[arrowsize=0.8 arrowhead=vee constraint=true]\n";

    // For NFA in our case, LR looks prettier
    if (!transformedDFAFlag) {
        s += "  rankdir=LR;\n";
    }

    // Add states
    if (getStartState() >= 0) {
        s += "  start [label=Start shape=plain]\n";
    }
    for (StateID stateID{0}, stateIDLimit = static_cast<StateID>(states.size());
         stateID < stateIDLimit; stateID = StateID{stateID + 1}) {
        auto &state = states[stateID];

        auto [dumpedString, finalFlag] = dumpState(stateID);
        std::string label = f.reverseEscaped(std::move(dumpedString));

        s += f.formatView("  %d [label=\"%d: %s\"", stateID, stateID,
                          label.c_str());

        if (finalFlag) {
            s += " peripheries=2";
        }

        //        if (highlightSet.contains(stateID)) {
        //            s += " color=red fontcolor=red";
        //            highlightSet.remove(stateID);
        //        }

        s += "]\n";
        if (stateID == getStartState()) {
            s += f.formatView("  start -> %d\n", stateID);
        }
        for (auto &tran : *state.transitions) {
            StateID destID = tran.destination;
            label = f.reverseEscaped(actions[tran.action]);
            s += f.formatView("  %d -> %d [label=\"%s\"", stateID, destID,
                              label.c_str());
            if (isEpsilonAction(tran.action))
                s += " constraint=false";
            //            if (tran.highlight) {
            //                s += " color=red fontcolor=red
            //                fontname=\"times-bold\""; tran.highlight = false;
            //            }
            s += "]\n";
        }
    }

    s += "}";

    return s;
}

// This function calculate closure and store information in place;
// Must be used inside toDFA(), because only then transitions are sorted.
void PushDownAutomaton::makeClosure(Closure &closure) const {
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

auto PushDownAutomaton::transit(
    Closure const &closure, ActionID actionID,
    std::vector<util::BitSet<StateID>> const &receiverVec) const
    -> ::std::optional<Closure> {
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

// For automatons which is not generated by toDFA().
std::pair<std::string, bool>
PushDownAutomaton::dumpState(StateID stateID) const {
    if (transformedDFAFlag) {
        return dumpClosure(closures[stateID]);
    }
    auto const &state = states[stateID];
    auto const &constraint = state.constraint;

    std::pair<std::string, bool> res = {
        (*kernelLabelMap)[state.productionID][state.rhsIndex], false};

    if (constraint->contains(endOfInputAction) &&
        (state.rhsIndex + 1 == (*kernelLabelMap)[state.productionID].size())) {
        res.second = true;
    }

    if (includeConstraints) {
        res.first += ", ";
        bool slashFlag = false;
        for (auto actionID : *constraint) {
            if (slashFlag)
                res.first += '/';
            res.first += actions[actionID];
            slashFlag = true;
        }
    }
    return res;
}

std::pair<std::string, bool>
PushDownAutomaton::dumpClosure(Closure const &closure) const {
    std::pair<std::string, bool> res;
    res.second = false;
    bool newLineFlag = false;
    for (auto stateID : closure) {
        if (newLineFlag)
            res.first += '\n';
        auto const &auxState = auxStates[stateID];
        auto const &constraint = auxState.constraint;

        res.first +=
            (*kernelLabelMap)[auxState.productionID][auxState.rhsIndex];

        if (!res.second && constraint->contains(endOfInputAction) &&
            (auxState.rhsIndex + 1 ==
             (*kernelLabelMap)[auxState.productionID].size())) {
            res.second = true;
        }

        if (includeConstraints) {
            res.first += ", ";
            bool slashFlag = false;
            for (auto actionID : *constraint) {
                if (slashFlag)
                    res.first += '/';
                res.first += actions[actionID];
                slashFlag = true;
            }
        }
        newLineFlag = true;
    }
    return res;
}

PushDownAutomaton PushDownAutomaton::toDFA() {
    PushDownAutomaton dfa{this->transitionSetProvider, this->kernelLabelMap};

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
    dfa.setEndOfInputAction(this->endOfInputAction);
    dfa.setEpsilonAction(this->epsilonAction);

    // The result is used by transit()
    std::vector<util::BitSet<StateID>> receiverVec(actions.size());

    for (StateID stateID{0}, stateIDLimit = static_cast<StateID>(states.size());
         stateID < stateIDLimit; stateID = StateID{stateID + 1}) {
        for (auto &tran : *states[stateID].transitions)
            receiverVec[tran.action].insert(stateID);
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

    dfa.closures = std::move(closureVec); // Move closures
    return dfa;
}

void PushDownAutomaton::setEpsilonAction(ActionID actionID) {
    epsilonAction = actionID;
}

void PushDownAutomaton::setEndOfInputAction(ActionID actionID) {
    endOfInputAction = actionID;
}

} // namespace gram