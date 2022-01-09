#include "util/display.h"
#include "grammar/grammar.h"
#include "automata/automata.h"
#include <exception>
#include <iostream>
#include <vector>

using std::vector;
using namespace lr;
using namespace am;
using namespace util;

void grammarToNFATest() {
    Grammar g = Grammar::fromStdin();
    TestDisplayer d("state.svg", 1000);
    Automaton m = Automaton::fromGrammar(g, d);
    // std::cout << Display::dumpAutomaton(m);
}

void grammarReadTest() {
    Grammar g = Grammar::fromStdin();
    std::cout << g.dump();
}

void automataTest() {
    Automaton m;
    int s1 = m.addState("1", true);
    int s2 = m.addState("2");
    m.setStartState(s1);
    m.addTransition(s1, s1, "b");
    m.addTransition(s2, s2, "b");
    m.addTransition(s1, s2, "a");
    m.addTransition(s2, s1, "a");
    m.setState(s2);
    std::cout << m.dump();
}

int main() try {
    grammarToNFATest();
    // grammarReadTest();
    // automataTest();
} catch (std::exception &e) {
    std::cerr << "[Error] " << e.what() << '\n';
}