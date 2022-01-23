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
}

void Automaton::setState(StateID state) { currentState = state; }

void Automaton::markFinalState(StateID state) {
  states[state].acceptable = true;
}

std::vector<State> const &Automaton::getAllStates() const { return states; }

std::vector<String> const &Automaton::getAllActions() const { return actions; }

StateID Automaton::getState() const { return currentState; }

StateID Automaton::getStartState() const { return startState; }

auto Automaton::getStatesBitmap() const -> std::vector<DFAState> const & {
  return statesBitmap;
}

auto Automaton::getFormerStates() const -> std::vector<State> const & {
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
    s += f.formatView("  %d [label=\"%d: %s\"", mappedStateID, mappedStateID,
                      label.c_str());
    if (state.acceptable) s += " peripheries=2";
    if (mappedStateID == getState())
      s += " fillcolor=yellow style=\"rounded,filled\"";
    if (stateHighlightSet.find(state.id) != stateHighlightSet.end())
      s += " color=red penwidth=2 fontcolor=red fontsize=18";
    s += "]\n";
    if (mappedStateID == getStartState()) {
      s += f.formatView("  start -> %d\n", mappedStateID);
    }
    for (auto &tran : state.trans) {
      StateID mappedDestID = posMap ? posMap[tran.second] : tran.second;
      label = f.reverseEscaped(actions[tran.first]);
      s += f.formatView("  %d -> %d [label=\"%s\"", mappedStateID, mappedDestID,
                        label.c_str());
      if (isEpsilonAction(tran.first)) s += " constraint=false";
      if (transitionHighlightSet.find({mappedStateID, mappedDestID}) !=
          transitionHighlightSet.end()) {
        s += " color=red penwidth=2 fontcolor=red fontsize=18";
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
void Automaton::toEpsClosure(DFAState &dfaState) const {
  std::stack<int> stack;
  for (auto s : dfaState) stack.push(static_cast<int>(s));

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
  DFAState res{static_cast<util::BitSet::size_type>(states.size())};

  DFAState receivers{actionReceivers[action]};
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

  toEpsClosure(res);
  return std::make_optional<DFAState>(std::move(res));
}

Automaton Automaton::toDFA() {
  // TODO: display support

  if (isDFA()) {
    // Already a DFA: make a copy of itself
    return *this;
  }

  // Build action-receivers vector
  actionReceivers.clear();
  actionReceivers.reserve(actions.size());
  for (size_t i = 0; i < actions.size(); ++i) {
    actionReceivers.emplace_back(
        static_cast<util::BitSet::size_type>(states.size()));
  }
  for (auto &state : states) {
    for (auto &tran : state.trans) actionReceivers[tran.first].add(state.id);
  }

  // A map which transforms a DFAState to its assigned ID
  std::unordered_map<DFAState, StateID> dfaStates;
  std::unordered_multimap<StateID, Transition> links;
  int dfaStateIndexCounter = -1;

  // All states in stack will finally go to `closures`.
  // We need to ensure that for S in `stack`, eps_closure(S) == S.
  // *This stack stores iterators.
  std::stack<decltype(dfaStates.cbegin())> stack;

  // Add start state
  StateID dfaStartState{-1};
  {
    DFAState start((util::BitSet::block_type)states.size());
    start.add(startState);
    toEpsClosure(start);
    auto insertResult =
        dfaStates.emplace(std::move(start), StateID{++dfaStateIndexCounter});
    stack.push(insertResult.first);
    dfaStartState = StateID{dfaStateIndexCounter};
  }

  while (!stack.empty()) {
    auto stateIter = stack.top();
    stack.pop();

    for (ActionID action{0}; static_cast<size_t>(action) < actions.size();
         action = ActionID{action + 1}) {
      if (action == epsilonAction) {
        continue;
      }
      // If this action is not acceptable, the entire test will
      // be skipped, thanks to parallel calculation of BitSet.
      auto result = transit(stateIter->first, action);
      if (result.has_value()) {
        auto &value = result.value();

        // Found a new state
        auto existingIter = dfaStates.find(value);
        if (existingIter == dfaStates.end()) {
          // Because we need to assign index from 0 continuously,
          // we must make sure the insertion will happen when we
          // use "++counter".
          auto iter =
              dfaStates
                  .emplace(std::move(value), StateID{++dfaStateIndexCounter})
                  .first;

          // Add link to this new state
          links.emplace(stateIter->second,
                        Transition{StateID{dfaStateIndexCounter}, action});

          stack.emplace(iter);
        } else {
          // Add link to this previous state
          links.emplace(stateIter->second,
                        Transition{existingIter->second, action});
        }
      }
    }
  }

  Automaton dfa;

  // Copy all actions to this new automaton.
  // Epsilon action is no longer useful but is still copyed,
  // so the index of other actions can stay the same.
  dfa.actions = this->actions;
  dfa.actIDMap = this->actIDMap;  // Maybe unnecessary

  // Add new states
  // Sort iterators by index
  std::vector<decltype(dfaStates.cbegin())> iterVec;
  iterVec.reserve(dfaStates.size());
  for (auto it = dfaStates.begin(); it != dfaStates.end(); ++it)
    iterVec.push_back(it);
  std::sort(iterVec.begin(), iterVec.end(),
            [](auto const &a, auto const &b) { return a->second < b->second; });

  // Add states to dfa
  dfa.states.reserve(dfaStates.size());
  for (size_t i = 0; i < iterVec.size(); ++i) {
    dfa.states.emplace_back(static_cast<StateID>(i), iterVec[i]->first.dump(),
                            false);
  }

  // Mark start state
  dfa.markStartState(dfaStartState);

  // Mark final state
  // Get a mask of final NFA states
  DFAState nfaFinalStates((util::BitSet::block_type)states.size());
  for (auto const &state : states) {
    if (state.acceptable) nfaFinalStates.add(state.id);
  }
  // Use final NFA states to find NFA final states
  for (auto &entry : dfaStates) {
    if (entry.first.hasIntersection(nfaFinalStates))
      dfa.markFinalState(entry.second);
  }

  // Add edges
  for (auto const &link : links) {
    dfa.addTransition(link.first, link.second.dest, link.second.action);
  }

  // Copy former NFA states to DFA, so we can trace original
  // states.
  std::vector<DFAState> nfaStatesBitmap;
  nfaStatesBitmap.reserve(dfa.states.size());
  for (auto &iter : iterVec) {
    nfaStatesBitmap.push_back(
        std::move(const_cast<util::BitSet &>(iter->first)));
  }
  dfa.statesBitmap = std::move(nfaStatesBitmap);  // Move bitmap
  dfa.formerStates = this->states;                // Copy vec

  // Can I use a moved vector again?
  // https://stackoverflow.com/a/9168917/13785815
  // Yes, but after calling clear()
  dfa.actionReceivers = std::move(actionReceivers);

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