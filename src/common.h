#ifndef LRPARSER_COMMON_H
#define LRPARSER_COMMON_H

#include <stdexcept>
#include <string>
#include <string_view>

// 1. Strings
using String = ::std::string;
using StringView = ::std::string_view;

// 2. Display
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

// 4. Parsers
enum ParserType { LR0, SLR, LALR, LR1 };

// 5. Arguments
struct LaunchArguments {
    bool launchSuccess = true;
    bool nodot = false;
    bool strict = false;
    bool exhaustInput = true;
    bool autoDefineTerminals = true;
    ParserType parserType = SLR;
    DisplayLogLevel logLevel = VERBOSE;
    String grammarFileName = "grammar.txt";
    String resultsDir = "results";
};

extern LaunchArguments launchArgs;

#endif