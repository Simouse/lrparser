#include <array>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <iostream>
#include <thread>
#include <vector>

#include "src/automata/automata.h"
#include "src/common.h"
#include "src/display/display.h"
#include "src/grammar/grammar.h"
#include "src/grammar/syntax.h"
#include "src/util/bitset.h"
#include "src/util/exec.h"

using namespace gram;
// using ft = FileOutputDisplay::FileType;

void grammarToNFATest() {
    // ConsoleDisplay cd;
    // FileOutputDisplay fd(".", 100, ft::SVG);
    // Grammar g = Grammar::fromFile("grammar.txt", cd);

    // SyntaxAnalysisLR0 lr0(g);
    // lr0.buildNFA(fd);
}

void automataTest() {
    Automaton m;
    StateID s1 = m.addState("1", true);
    StateID s2 = m.addState("2");
    m.markStartState(s1);
    m.addTransition(s1, s1, m.addAction("b"));
    m.addTransition(s2, s2, m.addAction("b"));
    m.addTransition(s1, s2, m.addAction("a"));
    m.addTransition(s2, s1, m.addAction("a"));
    m.setState(s2);
    std::cout << m.dump();
}

void NFA2DFATest() {
    // VoidDisplay d;
    // FileOutputDisplay fd(".", 0, ft::SVG);
    // ConsoleDisplay cd;
    Grammar g = Grammar::fromFile("grammar.txt");
    SyntaxAnalysisSLR slr(g);
    slr.buildNFA();
    slr.buildDFA();
    slr.buildParseTables();
    display(DisplayType::PARSE_TABLE, "Parse table", &slr);
}

int main()
// try
{
    util::Process::prevent_zombie();
    // grammarToNFATest();
    NFA2DFATest();
}
// catch (std::exception &e) {
//     std::cerr << "[Error] " << e.what() << std::endl;
// }