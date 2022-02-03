#ifndef LRPARSER_COMMON_H
#define LRPARSER_COMMON_H

#include <stdexcept>

// This file does not contain a namespace.
// Typing "namespace::" before some common definitions is what I want to
// avoid.🤔

// 1. enums
enum DisplayType {
    LOG,
    AUTOMATON,
    SYMBOL_TABLE,
    PARSE_TABLE,
    GRAMMAR_RULES,
    PARSE_STATES
};
// INFO:    Provide important text information for users
// VERBOSE: Users may want to have a look because it shows more details
// DEBUG:   For debugging purposes.
// ERR:     Error happened. Maybe it's caused by some illegal input.
enum DisplayLogLevel { INFO = 0, ERR, VERBOSE, DEBUG, LOG_LEVELS_COUNT };
enum SymbolType {
    NON_TERM = 0,
    TERM = 1, // For bool comparison
    UNCHECKED
};
enum ParserType { LR0, SLR, LALR, LR1 };
enum ActionID : int;
enum StateID : int;
enum TransitionID : int;
enum ProductionID : int;
// enum SymbolID : int;
// We may use static_cast when an element needs it, but a container
// containing those elements cannot be cast. And allowing BitSet<T1> and
// BitSet<T2> to be calculated together sounds wired.
using SymbolID = ActionID;

// Enum can compare with each other.
// template <class Enum> struct IntEnumLess {
//     static_assert(sizeof(int) == sizeof(Enum) && std::is_enum_v<Enum>,
//                   "Template argument type must be a int-based enum");
//     bool operator()(Enum a, Enum b) const {
//         return static_cast<int>(a) < static_cast<int>(b);
//     }
// };

// 2. global accessible functions
void display(DisplayType type, DisplayLogLevel level, const char *description,
             void const *pointer = nullptr, void const *auxPointer = nullptr);
void lrInit();
void lrCleanUp();
double upTimeInMilli();
void reportTime(const char *tag = nullptr);

// 3. Errors
struct UnimplementedError : public ::std::runtime_error {
    UnimplementedError() : ::std::runtime_error("Operation not implemented") {}
};

struct UnsupportedError : public ::std::runtime_error {
    UnsupportedError() : ::std::runtime_error("Operation not supported") {}
};

struct UnreachableCodeError : public ::std::logic_error {
    UnreachableCodeError() : ::std::logic_error("Unreachable code") {}
};

// 4. Arguments
struct LaunchArguments {
    bool launchSuccess = true;
    bool nodot = false;
    bool strict = false;
    bool exhaustInput = true;
    bool autoDefineTerminals = true;
    bool allowNonterminalAsInputs = false;
    ParserType parserType = SLR;
    DisplayLogLevel logLevel = VERBOSE;
    std::string grammarFileName = "grammar.txt";
    std::string resultsDir = "results";
};

extern LaunchArguments launchArgs;

#endif