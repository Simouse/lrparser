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
// void stepcode(const char *fmt, ...);
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
    bool noTest = false;
    bool noPDA = false;
    bool noPDALabel = false;
    ParserType parserType = SLR;
    DisplayLogLevel logLevel = VERBOSE;
    std::string grammarFileName = "grammar.txt";
    std::string resultsDir = ".";
    std::string bodyStartString = "->";
};

extern LaunchArguments launchArgs;

// 5. Constant strings
struct Constants {
    // constexpr static const char *table_new = "table.new";
    // constexpr static const char *table_target = "table.target";
    // constexpr static const char *table_resize = "table.resize";
    // constexpr static const char *table_put = "table.put";
    // constexpr static const char *list_new = "list.new";
    // constexpr static const char *list_target = "list.target";
    // constexpr static const char *list_push_front = "list.push_front";
    // constexpr static const char *list_push_back = "list.push_back";
    // constexpr static const char *list_pop_front = "list.pop_front";
    // constexpr static const char *list_pop_back = "list.pop_back";
    // constexpr static const char *automaton_new = "automaton.new";
    // constexpr static const char *automaton_target = "automaton.target";
    // constexpr static const char *automaton_add_node = "automaton.add_node";
    // constexpr static const char *automaton_merge_nodes = "automaton.merge_nodes";
    // constexpr static const char *automaton_add_edge = "automaton.add_edge";
    // constexpr static const char *tree_new = "tree.new";
    // constexpr static const char *tree_target = "tree.target";
    // constexpr static const char *tree_add_node = "tree.add_node";
    // constexpr static const char *tree_attach_node = "tree.attach_node";
    // constexpr static const char *mark_start = "mark.start";
    // constexpr static const char *mark_end = "mark.end";
    // constexpr static const char *display = "display";
    constexpr static const char *dot = "\xE2\x80\xA2"; // \xE2\x80\xA2 is "•"; \xE2\x97\x8F is "●" in UTF-8
    constexpr static const char *epsilon = "\xCE\xB5"; // \xCE\xB5 is "ε" in UTF-8
    constexpr static const char *end_of_input = "$";
};

#endif