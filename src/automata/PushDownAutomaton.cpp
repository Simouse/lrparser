#include "PushDownAutomaton.h"

#include <cassert>
#include <cstdio>
#include <optional>
#include <stack>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <utility>

#include "src/common.h"
#include "src/display/steps.h"
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

// s cannot be nullptr
static inline void fputs_escape(FILE *stream, const char *s) {
    using std::putc;
    char ch;
    while ((ch = *s++)) {
        switch (ch) {
        case '\'':
        case '\"':
        case '\\':
            putc('\\', stream);
            putc(ch, stream);
            break;
        case '\n':
            putc('\\', stream);
            putc('n', stream);
            break;
        case '\t':
            putc('\\', stream);
            putc('t', stream);
            break;
        default:
            putc(ch, stream);
            break;
        }
    }
}

void PushDownAutomaton::dump(FILE *stream) const {
    using std::fprintf;
    using std::putc;

    fprintf(stream,
            "digraph g%d {\n"
            "  charset=utf8;\n"
            "  splines=true;\n"
            "  graph[center=true];\n"
            "  edge[arrowsize=0.8 arrowhead=vee constraint=true];\n",
            (int)(upTimeInMilli() * 1000));

    if (launchArgs.noPDALabel) {
        fprintf(stream, "  node [shape=circle];\n");
    } else {
        fprintf(stream, "  node [shape=box style=rounded];\n");
    }

    // For NFA in our case, LR looks prettier
    if (!transformedDFAFlag) {
        fprintf(stream, "  rankdir=LR;\n");
    }

    // Add states
    if (getStartState() >= 0) {
        fprintf(stream, "  start [label=Start shape=plain];\n");
    }
    for (StateID stateID{0}, stateIDLimit = static_cast<StateID>(states.size());
         stateID < stateIDLimit; stateID = StateID{stateID + 1}) {
        auto &state = states[stateID];

        if (launchArgs.noPDALabel)
            fprintf(stream, "  %d [", stateID);
        else
            fprintf(stream, "  %d [label=\"%d: ", stateID, stateID);
        auto finalFlag = dumpState(stream, stateID);
        if (!launchArgs.noPDALabel)
            fprintf(stream, "\"");

        if (finalFlag) {
            fprintf(stream, " peripheries=2");
        }

        fprintf(stream, "];\n");
        if (stateID == getStartState()) {
            fprintf(stream, "  start -> %d;\n", stateID);
        }
        for (auto &tran : *state.transitions) {
            StateID destID = tran.destination;
            fprintf(stream, "  %d -> %d [label=\"", stateID, destID);
            fputs_escape(stream, actions[tran.action]);
            putc('\"', stream);
            if (isEpsilonAction(tran.action))
                fprintf(stream, " constraint=false");
            fprintf(stream, "];\n");
        }
    }

    fprintf(stream, "}");
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

bool PushDownAutomaton::dumpState(FILE *stream, StateID stateID) const {
    using std::fprintf;

    if (transformedDFAFlag) {
        return dumpClosure(stream, stateID);
    }
    auto const &state = states[stateID];
    auto const &constraint = state.constraint;

    bool finalFlag = false;

    if (constraint->contains(endOfInputAction) &&
        (state.rhsIndex + 1 == (*kernelLabelMap)[state.productionID].size())) {
        finalFlag = true;
    }

    if (launchArgs.noPDALabel) {
        return finalFlag;
    }

    fputs_escape(stream, (*kernelLabelMap)[state.productionID][state.rhsIndex]);

    if (includeConstraints) {
        fprintf(stream, ", ");
        bool slash = false;
        for (auto actionID : *constraint) {
            if (slash)
                fprintf(stream, "/");
            fputs_escape(stream, actions[actionID]);
            slash = true;
        }
    }
    return finalFlag;
}

std::string PushDownAutomaton::dumpStateString(StateID stateID) const {
    if (transformedDFAFlag) {
        return dumpClosureString(stateID);
    }

    auto const &state = states[stateID];
    auto const &constraint = state.constraint;

    std::string s;
    escape_ascii((*kernelLabelMap)[state.productionID][state.rhsIndex],
                 std::back_inserter(s), 0);

    if (this->includeConstraints) {
        s += ", ";
        bool slash = false;
        for (auto actionID : *constraint) {
            if (slash)
                s += "/";
            escape_ascii(actions[actionID], std::back_inserter(s), 0);
            slash = true;
        }
    }
    return s;
}

bool PushDownAutomaton::dumpClosure(FILE *stream, StateID closureID) const {
    bool finalFlag = false;
    bool newLineFlag = false;
    bool dumpDetail = !launchArgs.noPDALabel;
    auto const &closure = closures[closureID];
    // This part calculates return value, so it cannot be skipped.
    for (auto stateID : closure) {
        if (newLineFlag && dumpDetail)
            fprintf(stream, "\\n");
        auto const &auxState = auxStates[stateID];
        auto const &constraint = auxState.constraint;

        if (dumpDetail) {
            fputs_escape(
                stream,
                (*kernelLabelMap)[auxState.productionID][auxState.rhsIndex]);
        }

        if (!finalFlag && constraint->contains(endOfInputAction) &&
            (auxState.rhsIndex + 1 ==
             (*kernelLabelMap)[auxState.productionID].size())) {
            finalFlag = true;
        }

        if (includeConstraints && dumpDetail) {
            fprintf(stream, ", ");
            bool slashFlag = false;
            for (auto actionID : *constraint) {
                if (slashFlag)
                    fprintf(stream, "/");
                fputs_escape(stream, actions[actionID]);
                slashFlag = true;
            }
        }
        newLineFlag = true;
    }
    return finalFlag;
}

std::string PushDownAutomaton::dumpClosureString(StateID closureID) const {
    std::string s;
    bool finalFlag = false;
    bool newLineFlag = false;
    auto const &closure = closures[closureID];
    // This part calculates return value, so it cannot be skipped.
    for (auto stateID : closure) {
        if (newLineFlag)
            s += "\n";
        auto const &auxState = auxStates[stateID];
        auto const &constraint = auxState.constraint;

        escape_ascii(
            (*kernelLabelMap)[auxState.productionID][auxState.rhsIndex],
            std::back_inserter(s), 0);

        if (!finalFlag && constraint->contains(endOfInputAction) &&
            (auxState.rhsIndex + 1 ==
             (*kernelLabelMap)[auxState.productionID].size())) {
            finalFlag = true;
        }

        if (this->includeConstraints) {
            s += ", ";
            bool slash = false;
            for (auto actionID : *constraint) {
                if (slash)
                    s += "/";
                escape_ascii(actions[actionID], std::back_inserter(s), 0);
                slash = true;
            }
        }

        newLineFlag = true;
    }
    return s;
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
    // std::vector<Closure> closureVec;

    auto addNewState = [&closureIDMap, &dfa, &stack](Closure &&c) {
        auto stateIndex = static_cast<StateID>(dfa.closures.size());
        dfa.closures.push_back(std::move(c));
        closureIDMap.emplace(dfa.closures.back(), stateIndex);
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

    step::addState(stateIndex, dfa.dumpStateString(stateIndex));
    step::setStart(stateIndex);
    step::show("Add start state.");
    util::Formatter f;

    while (!stack.empty()) {
        auto stateID = stack.top();
        stack.pop();

        for (size_t i = 0; i < actions.size(); ++i) {
            auto actionID = static_cast<ActionID>(i);

            if (actionID == epsilonAction)
                continue;

            // If this action is not acceptable, the entire test will
            // be skipped.
            auto result = transit(dfa.closures[stateID], actionID, receiverVec);
            if (result.has_value()) {
                auto &value = result.value();
                auto existingIter = closureIDMap.find(value);

                if (existingIter == closureIDMap.end()) {
                    // Because LALR may combine constraints
                    auto nextStateID = addNewState(std::move(value));

                    step::addState(nextStateID, dfa.dumpStateString(nextStateID));
                    step::addEdge(stateID, nextStateID, actions[actionID]);
                    auto sv = f.formatView("Trans(s%d, %s) = s%d", stateID, actions[actionID], nextStateID);
                    step::show(sv);

                    // Add edge to this new state
                    dfa.addTransition(stateID, nextStateID, actionID);
                } else {
                    // Add edge to this previous state
                    dfa.addTransition(stateID, existingIter->second, actionID);

                    step::addEdge(stateID, existingIter->second, actions[actionID]);
                    auto sv = f.formatView("Trans(s%d, %s) = s%d", stateID, actions[actionID], existingIter->second);
                    step::show(sv);
                }
            }
        }
    }

    // dfa.closures = std::move(closureVec); // Move closures

    // TODO: set final


    return dfa;
}

void PushDownAutomaton::setEpsilonAction(ActionID actionID) {
    epsilonAction = actionID;
}

void PushDownAutomaton::setEndOfInputAction(ActionID actionID) {
    endOfInputAction = actionID;
}

} // namespace gram