#include <algorithm>
#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <future>
#include <mutex>
#include <vector>

#include "src/automata/PushDownAutomaton.h"
#include "src/common.h"
#include "src/grammar/Grammar.h"
#include "src/parser/LRParser.h"
#include "src/parser/SLRParser.h"
#include "src/util/Formatter.h"
#include "src/util/Process.h"
#include "src/display/steps.h"

namespace fs = std::filesystem;
using namespace gram;
using proc = util::Process;

// Log strings
static std::array<const char *, LOG_LEVELS_COUNT> logs = {nullptr};
static int automatonCounter = 0;
static std::chrono::steady_clock globalClock;
static decltype(globalClock.now()) startUpTime;
FILE *stepFile;

double upTimeInMilli() {
    auto now = globalClock.now();
    std::chrono::duration<double, std::milli> duration = now - startUpTime;
    return duration.count();
}

void reportTime(const char *tag) {
    static util::Formatter debugFormatter;
    display(LOG, DEBUG,
            debugFormatter
                .formatView("%-20s: %.1f ms has elapsed since startup",
                            tag ? tag : "NUL", upTimeInMilli())
                .data());
}

// void stepcode(const char *fmt, ...) {
//     va_list ap;
//     va_start(ap, fmt);
//     std::vfprintf(stepFile, fmt, ap);
//     va_end(ap);
// }

void lrInit() {
    startUpTime = globalClock.now();

    logs[DisplayLogLevel::INFO] = "INFO";
    logs[DisplayLogLevel::VERBOSE] = "VERBOSE";
    logs[DisplayLogLevel::DEBUG] = "DEBUG";
    logs[DisplayLogLevel::ERR] = "ERROR";

    util::Process::prevent_zombie();

    fs::path outPath = launchArgs.resultsDir;
    if (!fs::exists(outPath))
        fs::create_directories(outPath);

    outPath /= "steps.py";
    std::string outName = outPath.string();
    stepFile = std::fopen(outName.c_str(), "w");
}

void lrCleanUp() {
    if (!launchArgs.launchSuccess) {
        return;
    }
    stepFinish();
    std::fclose(stepFile);
}

static void handleLog(const char *description, DisplayLogLevel level) {
    if (launchArgs.logLevel < level)
        return;
    if (level == INFO)
        printf("> %s\n", description);
    else
        printf("[%-7s] %s\n", logs[level], description);
}

static std::string generateLogLine(const char *description,
                                   DisplayLogLevel level) {
    if (launchArgs.logLevel < level || !description)
        return "";
    util::Formatter f;
    std::string s;
    if (level == INFO)
        s += f.formatView("> %s\n", description);
    else
        s += f.formatView("[%-7s] %s\n", logs[level], description);
    return s;
}

static void handleParseStates(const char *description, DisplayLogLevel logLevel,
                              gram::LRParser const *lr) {
    util::Formatter f;
    std::string s;
    auto const &stateStack = lr->getStateStack();
    auto const &symbolStack = lr->getSymbolStack();
    auto const &inputQueue = lr->getInputQueue();
    auto const &symbols = lr->getGrammar().getAllSymbols();
    bool commaFlag;

    s += generateLogLine(description, logLevel);
    s += "State stack : Bottom->| ";
    commaFlag = false;
    for (auto i : stateStack) {
        s += f.formatView(commaFlag ? ",%d" : "%d", i);
        commaFlag = true;
    }
    s += "\nSymbol stack: Bottom->| ";
    commaFlag = false;
    for (auto i : symbolStack) {
        s += f.formatView(commaFlag ? ",%s" : "%s", symbols[i].name.c_str());
        commaFlag = true;
    }
    s += "\nInput queue : Front ->| ";
    commaFlag = false;
    for (auto i : inputQueue) {
        s += f.formatView(commaFlag ? ",%s" : "%s", symbols[i].name.c_str());
        commaFlag = true;
    }
    if (lr->hasMoreInput())
        s += "...";

    printf("%s\n", s.c_str());
}

static void handleParseTable(const char *description, DisplayLogLevel logLevel,
                             gram::LRParser const *lr) {
    constexpr const int indexWidth = 8;
    constexpr const int actionWidth = 8;
    constexpr const int gotoWidth = 6;
    constexpr const char *dashline = "--------";

    auto const &grammar = lr->getGrammar();
    auto const &symbols = grammar.getAllSymbols();
    auto parseTableRowNumber = lr->getParseTable().size();
    auto epsilonID = grammar.getEpsilonSymbol().id;

    util::Formatter f;
    std::string outputString;

    outputString += generateLogLine(description, logLevel);

    // Split symbols into terminals and non-terminals:
    // 1. Do not add epsilon.
    // 2. `$` must be included, but it's already in `symbols` so there's
    // no special care.
    std::vector<int> termVec, nontermVec;
    termVec.reserve(symbols.size());
    nontermVec.reserve(symbols.size());
    for (auto const &symbol : symbols) {
        if (symbol.type == SymbolType::TERM && symbol.id != epsilonID)
            termVec.push_back(symbol.id);
        else if (symbol.type == SymbolType::NON_TERM)
            nontermVec.push_back(symbol.id);
    }

    // Print header
    int allActionsWidth =
        (actionWidth + 2) * static_cast<int>(nontermVec.size());
    int allGotosWidth = (gotoWidth + 2) * static_cast<int>(termVec.size());
    outputString +=
        f.formatView("%*s %*s %*s\n", indexWidth, "States", allActionsWidth,
                     "Actions", allGotosWidth, "Gotos");
    outputString +=
        f.formatView("%*s %*s %*s\n", indexWidth, dashline, allActionsWidth,
                     dashline, allGotosWidth, dashline);

    // Print symbol names
    outputString += f.formatView("%*s ", indexWidth, "");
    for (int terminal : termVec) {
        outputString +=
            f.formatView("|%*s ", actionWidth, symbols[terminal].name.c_str());
    }
    for (int nonterm : nontermVec) {
        outputString +=
            f.formatView("|%*s ", gotoWidth, symbols[nonterm].name.c_str());
    }
    outputString += f.formatView("\n");

    // Print table entries
    for (int i = 0; static_cast<size_t>(i) < parseTableRowNumber; ++i) {
        outputString += f.formatView("%*d ", indexWidth, i);
        for (int terminal : termVec) {
            std::string s =
                lr->dumpParseTableEntry(StateID{i}, ActionID{terminal});
            outputString += f.formatView("|%*s ", actionWidth, s.c_str());
        }
        for (int nonterm : nontermVec) {
            std::string s =
                lr->dumpParseTableEntry(StateID{i}, ActionID{nonterm});
            outputString += f.formatView("|%*s ", gotoWidth, s.c_str());
        }
        outputString += f.formatView("\n");
    }

    // No trailing '\n' is needed
    printf("%s", outputString.c_str());
}

static void handleAutomaton(const char *description, DisplayLogLevel logLevel,
                            const char *prefix,
                            PushDownAutomaton const *automaton) {
    util::Formatter f;
    handleLog(description, logLevel);

    if (launchArgs.noPDA)
        return;

    fs::path gvFilePath = launchArgs.resultsDir;
    gvFilePath /= f.formatView("%s.%d.gv", prefix, ++automatonCounter);
    std::string gvFileName = gvFilePath.string();

    FILE *file = std::fopen(gvFileName.c_str(), "w");
    automaton->dump(file);
    fprintf(file, "\n");
    std::fclose(file);
}

static void handleSymbolTable(const char *description, DisplayLogLevel logLevel,
                              gram::Grammar const *grammar) {
    constexpr const char *lineFmt = "%*s %10s %*s %16s\n";
    constexpr const int nameWidth = 10;
    constexpr const int firstWidth = 20;
    constexpr const char *dashline = "--------";

    auto &g = *grammar;
    util::Formatter f;
    std::string outputString;

    outputString += generateLogLine(description, logLevel);
    outputString += f.formatView(lineFmt, nameWidth, "Name", "Nullable",
                                 firstWidth, "First{}", "Follow{}");
    outputString += f.formatView(lineFmt, nameWidth, dashline, dashline,
                                 firstWidth, dashline, dashline);

    auto const &epsilon = g.getEpsilonSymbol();
    for (auto &symbol : g.getAllSymbols()) {
        // Do not output terminal information
        if (symbol.type == SymbolType::TERM) {
            continue;
        }
        std::string nullable = g.dumpNullable(symbol);
        std::string firstSet = g.dumpFirstSet(symbol);
        std::string followSet = g.dumpFollowSet(symbol);
        int namew = (epsilon.id == symbol.id) ? nameWidth + 1 : nameWidth;
        int firstw = (firstSet.find(Constants::epsilon) != std::string::npos)
                         ? firstWidth + 1
                         : firstWidth;
        outputString +=
            f.formatView(lineFmt, namew, symbol.name.c_str(), nullable.c_str(),
                         firstw, firstSet.c_str(), followSet.c_str());
    }

    // No trailing '\n' is needed
    printf("%s", outputString.c_str());
}

static void handleGrammarRules(const char *description,
                               DisplayLogLevel logLevel,
                               gram::Grammar const *grammar) {
    std::string s = grammar->dump();
    std::string logLine = generateLogLine(description, logLevel);
    printf("%s%s\n", logLine.c_str(), s.c_str());
}

void display(DisplayType type, DisplayLogLevel level, const char *description,
             void const *pointer, void const *auxPointer) {
    switch (type) {
    case DisplayType::AUTOMATON:
        handleAutomaton(description, level, (const char *)auxPointer,
                        (PushDownAutomaton const *)pointer);
        break;
    case DisplayType::SYMBOL_TABLE:
        handleSymbolTable(description, level, (Grammar const *)pointer);
        break;
    case DisplayType::LOG:
        handleLog(description, level);
        break;
    case DisplayType::PARSE_TABLE:
        handleParseTable(description, level, (LRParser const *)pointer);
        break;
    case DisplayType::GRAMMAR_RULES:
        handleGrammarRules(description, level, (Grammar const *)pointer);
        break;
    case DisplayType::PARSE_STATES:
        handleParseStates(description, level, (LRParser const *)pointer);
        break;
    default:
        fprintf(stderr, "[ERROR  ] Unknown display type. Check your code.\n");
        exit(1);
        break;
    }
}