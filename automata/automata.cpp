#include "automata/automata.h"
#include "grammar/grammar.h"
#include "util/display.h"
#include "util/formatter.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using lr::Grammar;
using std::string;
using std::vector;
// using util::Display;
using util::Displayer;

namespace am {

int Automaton::addState(string desc, bool acceptable) {
    int id = (int)states.size();
    states.emplace_back(id, std::move(desc), acceptable);
    return id;
}

void Automaton::addTransition(int from, int to, const std::string &action) {
    states[from].trans.emplace_back(to, action.c_str(), action.size());
}

void Automaton::setStartState(int state) { startState = state; }

// TODO: Unsafe, consider remove this from public
void Automaton::setState(int state) { currentState = state; }

auto Automaton::getStates() const -> decltype(states) const & { return states; }

int Automaton::stateNow() const { return currentState; }

int Automaton::getStartState() const { return startState; }

// TODO: epsilon support
// TODO: add displayer support
Automaton Automaton::fromGrammar(const Grammar &g, Displayer &d) {
    // constexpr char DOT = '*'; // May change dot shape in the future
    const char *DOT = g.getDotSign();
    Automaton m;
    auto &symbols = g.getAllSymbols();
    auto &start = g.getStartSymbol();

    // States from grammar
    auto &rules = g.getRules();
    for (auto &entry : rules) {
        // S -> aB : S0 = {S -> .aB, S -> a.B, S -> aB.}
        auto &rule = entry.second;
        int nid = entry.first;

        for (int rhsIndex = 0; rhsIndex < rule.size(); ++rhsIndex) {
            if (g.isEpsilonRule(nid, rhsIndex)) {
                // TODO:
                //
                continue;
            }
            auto &ruleRhs = rule[rhsIndex];
            vector<int> stateIds(ruleRhs.size() + 1);
            for (int dotPos = 0; dotPos <= ruleRhs.size(); ++dotPos) {
                string name = g.getLR0StateName(nid, rhsIndex, dotPos);
                int stateId = m.addState(name);
                stateIds[dotPos] = stateId;
            }
            // Link states
            for (int dotPos = 0; dotPos < ruleRhs.size(); ++dotPos) {
                int actionId = ruleRhs[dotPos]; // symid == actionid
                m.addTransition(stateIds[dotPos], stateIds[dotPos + 1],
                                symbols[ruleRhs[dotPos]].name);
            }
        }
    }

    d.updateAutomaton("Generate states from each grammar rule", m);

    // Start state: Create S' for S
    // TODO: make name unique
    string extStartName = start.name + '\'';
    int extS0 = m.addState(extStartName + " -> " + DOT + " " + start.name);
    m.setStartState(extS0);
    int extS1 =
        m.addState(extStartName + " -> " + start.name + " " + DOT, true);
    m.addTransition(extS0, extS1, start.name);
    m.setStartState(extS0);

    d.updateAutomaton("Create a new state for start symbol", m);

    return m;
}

auto Automaton::dump() const -> std::string {
    std::string s;
    util::Formatter f;
    // auto &actions = m.getActions();
    auto &states = getStates();

    s += "digraph G {\n";
    s += "  rankdir=LR;\n";
    s += "  node [shape=box style=rounded]\n";
    if (getStartState() >= 0) {
        s += "  start [label=Start shape=plain]\n";
    }
    for (auto &state : states) {
        s += f.bufferAfterFormat("  %d [label=\"%s\"", state.id,
                                 &state.label[0]);
        if (state.acceptable) {
            s += " peripheries=2";
        }
        if (state.id == stateNow()) {
            s += " fillcolor=yellow style=filled";
        }
        s += "]\n";
        if (state.id == getStartState()) {
            s += f.bufferAfterFormat(" start -> %d\n", state.id);
        }
        for (auto &tran : state.trans) {
            s += f.bufferAfterFormat("  %d -> %d [label=\"%s\"]\n", state.id,
                                     tran.dest, tran.action.c_str());
        }
    }
    s += "}\n";

    return s;
}

} // namespace am