#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <string>

namespace lr {
class Grammar;
}

namespace am {
class Automaton;
}

namespace util {

// Get information about analysis process
class Displayer {
  public:
    virtual void updateAutomaton(const char *description,
                                 const am::Automaton &m) = 0;
};

class TestDisplayer : public Displayer {
    std::string dotCmd;
    long long delay;

  public:
    TestDisplayer(const char *outPath, long long delayInMs)
        : dotCmd(std::string("dot -Tsvg -o") + outPath), delay(delayInMs) {}
    void updateAutomaton(const char *description,
                         const am::Automaton &m) override;
};

// struct Display {
// Dump: no intermediate explanation
// static std::string dumpGrammar(const lr::Grammar &g);
// static std::string dumpAutomaton(const am::Automaton &m);

// Explain: work with displayer
// static am::Automaton explainLR0AmCreation(const lr::Grammar &g,
//   Displayer &d);
}; // namespace util
// } // namespace util

#endif