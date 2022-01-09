#include "util/display.h"
#include "automata/automata.h"
#include "util/formatter.h"
#include "grammar/grammar.h"
#include "port/pipe.h"
#include <chrono>
#include <cstdarg>
#include <string>
#include <thread>
#include <vector>
#include <iostream>
#include <cstdlib>

using am::Automaton;
using std::string;
using std::vector;
using std::vsnprintf;
// using util::Display;
using util::Formatter;

namespace util {

// auto Display::dumpGrammar(const lr::Grammar &g) -> std::string {
//     string s;
//     auto &symVec = g.getAllSymbols();
//     auto &rules = g.getRules();
//     const char *typeStr[] = {"  Nonterminals:\n", "  Terminals:\n"};
//     vector<int> classifiedSyms[2];

//     for (auto &sym : symVec) {
//         classifiedSyms[(int)sym.type].push_back(sym.id);
//     }
//     s += "Symbols:\n";
//     for (int i = 1; i >= 0; --i) {
//         s += typeStr[i];
//         for (int symid : classifiedSyms[i]) {
//             auto &sym = symVec[symid];
//             s += "    { name: \"" + sym.name + "\", ";
//             s += "id: " + std::to_string(sym.id) + " }";
//             if (sym.id == g.getStartSymbol().id) {
//                 s += " [START]";
//             }
//             s += '\n';
//         }
//     }

//     s += "Rules:\n";
//     for (auto &ruleEntry : rules) {
//         auto &rule = ruleEntry.second;
//         int nid = ruleEntry.first; // Nonterminal id

//         if (rule.empty())
//             continue; // No rule for this symbol
//         s += "  " + symVec[nid].name + " ->";
//         bool firstRule = true;
//         for (auto &ruleItem : rule) {
//             if (!firstRule) {
//                 s += "    |";
//             }
//             for (int ruleRhs : ruleItem) {
//                 s += ' ';
//                 s += symVec[ruleRhs].name;
//             }
//             s += '\n';
//             firstRule = false;
//         }
//     }

//     return s;
// }

// std::string Display::dumpAutomaton(const Automaton &m) {
//     std::string s;
//     Formatter f;
//     // auto &actions = m.getActions();
//     auto &states = m.getStates();

//     s += "digraph G {\n";
//     s += "  rankdir=LR;\n";
//     s += "  node [shape=box style=rounded]\n";
//     s += "  start [label=Start shape=plain]\n";
//     for (auto &state : states) {
//         s += f.bufferAfterFormat("  %d [label=\"%s\"", state.id,
//                                  &state.label[0]);
//         if (state.acceptable) {
//             s += " peripheries=2";
//         }
//         if (state.id == m.stateNow()) {
//             s += " fillcolor=yellow style=filled";
//         }
//         s += "]\n";
//         if (state.id == m.getStartState()) {
//             s += f.bufferAfterFormat(" start -> %d\n", state.id);
//         }
//         for (auto &tran : state.trans) {
//             s += f.bufferAfterFormat("  %d -> %d [label=\"%s\"]\n", state.id,
//                                      tran.dest, tran.action.c_str());
//         }
//     }
//     s += "}\n";

//     return s;
// }

// Automaton Display::explainLR0AmCreation(const lr::Grammar &g,
//                                         Displayer &d) {
//     return Automaton::fromGrammar(g, d);
// }

void TestDisplayer::updateAutomaton(const char *description,
                                    const am::Automaton &m) {
    auto h = PipeActions::open(dotCmd.c_str(), "w");
    string s = m.dump(); // s should already have a '\n'
    PipeActions::printf(h, "%s", s.c_str());
    PipeActions::close(h);
    std::printf("> %s\n", description);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
}

} // namespace util