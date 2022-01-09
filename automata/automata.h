#ifndef __AUTOMATA_H__
#define __AUTOMATA_H__

#include "util/str.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace lr {
class Grammar;
}

namespace util {
class Displayer;
}

namespace am {

struct Transition {
    int dest;         // Next state
    util::Str action; // Action, which is a symbol in syntax

    Transition(int dest, const char *actstr, unsigned int actlen)
        : dest(dest), action(actstr, actlen) {}
};

struct State {
    bool acceptable;
    int id;
    std::vector<Transition> trans;
    std::string label;

    // Pass string by value so literals like "John" will not
    // cause an extra copy
    State(int id, std::string label, bool acceptable)
        : acceptable(acceptable), id(id), label(std::move(label)) {}
};

class Automaton {
    int startState = -1;
    int currentState = -1;
    std::vector<State> states;

  public:
    // Use std::move inside to avoid 2 times copy from const char *
    int addState(std::string desc, bool acceptable = false);
    void addTransition(int from, int to, const std::string &action);
    void addTransition(int from, int to, const char *actstr,
                       const char *actlen);
    void setStartState(int state);

    void toDFA();
    void toDFA(util::Displayer &d); // Displayer may change state like a stream?

    // TODO: Unsafe, consider remove this from public
    void setState(int state);

    // Const
    [[nodiscard]] auto getStates() const -> decltype(states) const &;
    [[nodiscard]] int stateNow() const;
    [[nodiscard]] int getStartState() const;

    [[nodiscard]] std::string dump() const;

    // Use automaton with other tools
    static Automaton fromGrammar(const lr::Grammar &g,
                                          util::Displayer &d);
};
} // namespace am

#endif