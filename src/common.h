#ifndef LRPARSER_COMMON_H
#define LRPARSER_COMMON_H

#include <stdexcept>
#include <string>
#include <string_view>

// 1. Strings
using String = ::std::string;
using StringView = ::std::string_view;

// 2. Display

// Only define types associated with the last void * argument.
// How to display those types are not specified.
enum DisplayType {
  LOG = 0,  // So nullptr can be LOG
  AUTOMATON,
  SYMBOL_TABLE,
  PARSE_TABLE,
  GRAMMAR_RULES,
  LR_STATE_STACK,
  LR_SYMBOL_STACK
};

// INFO:    Provide important text information for users
// VERBOSE: Users may want to have a look because it shows more details
// DEBUG:   For debugging purposes.
// ERR:     Error happened. Maybe it's caused by some illegal input.
enum DisplayLogLevel { INFO = 0, ERR, VERBOSE, DEBUG, LOG_LEVELS_COUNT };

struct DisplayData {
  union {
    void *pointer;
    DisplayLogLevel logLevel;
  };
  DisplayData(void *p) : pointer(p) {}
  DisplayData(DisplayLogLevel level) : logLevel(level) {}
};

// Data may be NULL if you don't need it.
void display(DisplayType type, DisplayData data, const char *description);
void lrInit();
void lrCleanUp();

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
  DisplayLogLevel logLevel = DEBUG;
  String grammarFileName;
  String resultsDir = "results";
};

extern LaunchArguments launchArgs;

#endif