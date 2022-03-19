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
    // bool autoDefineTerminals = true;
    bool noTest = false;
    // bool noPDA = false;
    bool noPDALabel = false;
    ParserType parserType = SLR;
    DisplayLogLevel logLevel = VERBOSE;
    std::string grammarFileName = "grammar.txt";
    std::string resultsDir = ".";
    std::string sep = "->";
};

extern LaunchArguments launchArgs;

// 5. Constant strings
struct Constants {
    constexpr static const char *dot = "\xE2\x80\xA2"; // \xE2\x80\xA2 is "•"; \xE2\x97\x8F is "●" in UTF-8
    constexpr static const char *epsilon = "\xCE\xB5"; // \xCE\xB5 is "ε" in UTF-8
    constexpr static const char *end_of_input = "$";
};

// 6. Format
template<class OutIter>
OutIter escape_all(std::string_view const& s, OutIter out, char quote) {
  if (quote)
    *out++ = quote;

  for (unsigned char c : s) {
     if (' ' <= c && c <= '~' && c != '\\' && c != '"') {
      *out++ = c;
    } 
    else {
      *out++ = '\\';
      switch(c) {
      case '"':  *out++ = '"';  break;
      case '\\': *out++ = '\\'; break;
      case '\t': *out++ = 't';  break;
      case '\r': *out++ = 'r';  break;
      case '\n': *out++ = 'n';  break;
      case '\'': *out++ = '\''; break;
      default:
        char const* const hexdig = "0123456789ABCDEF";
        *out++ = 'x';
        *out++ = hexdig[c >> 4];
        *out++ = hexdig[c & 0xF];
      }
    }
  }

  if (quote)
    *out++ = quote;

  return out;
}

template<class OutIter>
OutIter escape_ascii(std::string_view const& s, OutIter out, char quote) {
  if (quote)
    *out++ = quote;
  for (unsigned char c : s) {
     if (' ' <= c && c <= '~' && c != '\\' && c != '"') {
      *out++ = c;
    } 
    else {
      switch(c) {
      case '"':  *out++ = '\\'; *out++ = '"';  break;
      case '\\': *out++ = '\\'; *out++ = '\\'; break;
      case '\t': *out++ = '\\'; *out++ = 't';  break;
      case '\r': *out++ = '\\'; *out++ = 'r';  break;
      case '\n': *out++ = '\\'; *out++ = 'n';  break;
      case '\'': *out++ = '\\'; *out++ = '\''; break;
      default:   *out++ = c;
      }
    }
  }
  if (quote)
    *out++ = quote;
  return out;
}

#endif