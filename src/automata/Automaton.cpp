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

StateID
Automaton::addState(String desc,
                    std::shared_ptr<util::BitSet<ActionID>> constraints) {
    auto id = static_cast<StateID>(states.size());
    states.emplace_back(id, std::move(desc), constraints);
    return id;
}

void Automaton::addTransition(StateID from, StateID to, ActionID action) {
    auto &trans = states[from].getTransitionSet();

    if (trans.containsAction(action)) {
        this->multiDestFlag = true;
    }

    trans.insert(Transition{to, action});
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

void Automaton::setDumpFlag(bool flag) {
    includeConstraints = flag;
}

void Automaton::markStartState(StateID state) {
    startState = state;
    currentState = state;

    highlightState(state);
}

void Automaton::setState(StateID state) { currentState = state; }

void Automaton::markFinalState(StateID state) {
    states[state].endable = true;

    highlightState(state);
}

std::vector<State> const &Automaton::getAllStates() const { return states; }

auto Automaton::getStatesMutable() -> std::vector<State> & { return states; }

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
    if (it != actIDMap.end())
        return it->second;
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

        String label = f.reverseEscaped(dumpState(state.id));
        s += f.formatView("  %d [label=\"%d: %s\"", mappedStateID,
                          mappedStateID, label.c_str());

        if (state.endable)
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
        for (auto &tran : state.getTransitionSet()) {
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
        auto const &trans = states[s].getTransitionSet();
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
auto Automaton::transit(StateClosure const &closure, ActionID action) const
    -> ::std::optional<StateClosure> {
    assert(action != epsilonAction);

    bool found = false;
    StateClosure res{states.size()};

    // Copy bitset
    util::BitSet receivers = actionReceivers[action];
    receivers &= closure;
    for (auto state : receivers) {
        // This state can receive current action
        auto const &trans = states[state].getTransitionSet();
        auto it = trans.lowerBound(action);
        auto end = trans.end();
        for (; it != end && it->action == action; ++it) {
            res.insert(it->to);
            found = true;
        }
    }

    if (!found)
        return {};

    makeClosure(res);
    return std::make_optional<StateClosure>(std::move(res));
}

String Automaton::dumpState(StateID stateID) const {
    String s = states[stateID].getLabel();
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
        s += formerStates[stateID].getLabel();
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

Automaton Automaton::toDFA() {
    Automaton dfa;

    // Make a copy if this is already a DFA
    if (isDFA()) {
        dfa = *this;
        dfa.separateKernels();
        dfa.transformedDFAFlag = true;
        return dfa;
    }

    // Copy all actions to this new automaton. There are still some data
    // being built, they will be moved before this function returns.
    // NOTE: Epsilon action is no longer useful but is still copyed,
    // so the index of other actions can stay the same.
    dfa.actions = this->actions;
    dfa.actIDMap = this->actIDMap;
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
        for (auto &tran : state.getTransitionSet())
            actionReceivers[tran.action].insert(state.id);
    }

    // States that need to be processed.
    // We need to ensure that for S in `stack`, makeClosure(S) == S.
    std::stack<StateID> stack;
    std::unordered_map<StateClosure, StateID> closureIDMap;
    std::vector<StateClosure> closureVec;

    auto addNewState = [&closureIDMap, &closureVec, &dfa,
                        &stack](StateClosure &&sc) {
        auto stateIndex = static_cast<StateID>(closureVec.size());
        closureVec.push_back(std::move(sc));
        closureIDMap.emplace(closureVec.back(), stateIndex);
        dfa.addState(dfa.dumpStateClosure(closureVec.back()), nullptr);
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
    util::BitSet<StateID> finalStateMask{states.size()};
    // TODO: endable is not reliable!
    for (auto const &state : states) {
        if (state.endable) // This property should be handled when building NFA
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
    auto const &trans = states[currentState].getTransitionSet();
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

void Automaton::separateKernels() {
    for (auto &state : this->states) {
        if (state.kernel)
            state.kernel = std::make_shared<StateKernel>(*state.kernel);
    }
    for (auto &state : this->formerStates) {
        if (state.kernel)
            state.kernel = std::make_shared<StateKernel>(*state.kernel);
    }
}

Automaton Automaton::deepClone() const {
    Automaton res = *this;
    res.separateKernels();
    return res;
}

} // namespace gram