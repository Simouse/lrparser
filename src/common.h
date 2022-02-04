#ifndef LRPARSER_COMMON_H
#define LRPARSER_COMMON_H

#include <stdexcept>
#include <string>

// The only header file which does not use namespace.

// 1. Enums
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
// // enum SymbolID : int;
// We may use static_cast when an element needs it, but a container
// containing those elements cannot be cast. And allowing BitSet<T1> and
// BitSet<T2> to be calculated together sounds wired.
using SymbolID = ActionID;

// 2. Global accessible functions
void display(DisplayType type, DisplayLogLevel level, const char *description,
             void const *pointer = nullptr, void const *auxPointer = nullptr);
void lrInit();
void lrCleanUp();
double upTimeInMilli();
void reportTime(const char *tag = nullptr);

// 3. Errors
struct UnimplementedError : public std::runtime_error {
    UnimplementedError() : std::runtime_error("Operation not implemented") {}
};

struct UnsupportedError : public std::runtime_error {
    UnsupportedError() : std::runtime_error("Operation not supported") {}
};

struct UnreachableCodeError : public std::logic_error {
    UnreachableCodeError() : std::logic_error("Unreachable code") {}
};

// 4. Arguments
struct LaunchArguments {
    bool launchSuccess = true;
    bool nodot = false;
    bool strict = false;
    bool exhaustInput = true;
    bool autoDefineTerminals = true;
    ParserType parserType = SLR;
    DisplayLogLevel logLevel = VERBOSE;
    std::string grammarFileName = "grammar.txt";
    std::string resultsDir = "results";
};

extern LaunchArguments launchArgs;

#endif