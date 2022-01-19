#include <array>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <iostream>
#include <thread>
#include <vector>

#include "src/automata/automata.h"
#include "src/common.h"
#include "src/grammar/gram.h"
#include "src/grammar/lr.h"
#include "src/util/exec.h"

using namespace gram;

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
    Grammar g = Grammar::fromFile("grammar.txt");
    SyntaxAnalysisSLR slr(g);
    slr.buildNFA();
    slr.buildDFA();
    slr.buildParseTables();
    slr.test(std::cin);
}

int main() {
    util::Process::prevent_zombie();
    NFA2DFATest();
}