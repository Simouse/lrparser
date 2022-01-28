#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <future>
#include <mutex>
#include <vector>

#include "src/automata/Automaton.h"
#include "src/common.h"
#include "src/grammar/Grammar.h"
#include "src/parser/LRParser.h"
#include "src/parser/SLRParser.h"
#include "src/util/Formatter.h"
#include "src/util/Process.h"

namespace fs = std::filesystem;
using namespace gram;
using proc = util::Process;

// If vector of future is stored in an object or it's class static storage
// space, it cannot be initialized. Then a compile-time error occurs.
static std::vector<std::future<void>> futures;

// Designated assignment is supported only in C++20...
static std::array<const char *, DisplayLogLevel::LOG_LEVELS_COUNT> logs = {
    nullptr};
static int automatonCounter = 0;
static std::chrono::steady_clock globalClock;
static decltype(globalClock.now()) startUpTime;

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

void lrInit() {
    startUpTime = globalClock.now();

    logs[DisplayLogLevel::INFO] = "INFO";
    logs[DisplayLogLevel::VERBOSE] = "VERBOSE";
    logs[DisplayLogLevel::DEBUG] = "DEBUG";
    logs[DisplayLogLevel::ERR] = "ERROR";

    futures.reserve(16);

    util::Process::prevent_zombie();

    fs::path outDir = launchArgs.resultsDir;
    if (!fs::exists(outDir)) fs::create_directories(outDir);
}

void lrCleanUp() {
    if (!launchArgs.launchSuccess) return;

    auto clock = std::chrono::steady_clock();
    auto t1 = clock.now();

    // Future waiting is automatic in it's deinit function.
    // Wait here so we can get the time it costs.
    for (auto const &future : futures) future.wait();

    auto t2 = clock.now();
    std::chrono::duration<double, std::milli> duration = t2 - t1;
    if (!launchArgs.nodot) {
        util::Formatter f;
        const char *description =
            f.formatView(
                 "%.1f ms has been spent in waiting for threads to finish",
                 duration.count())
                .data();
        display(LOG, DEBUG, description);
    }
}

static void handleLog(const char *description, DisplayLogLevel level) {
    if (launchArgs.logLevel < level) return;
    if (level == INFO)
        printf("> %s\n", description);
    else
        printf("[%-7s] %s\n", logs[level], description);
}

static String generateLogLine(const char *description, DisplayLogLevel level) {
    if (launchArgs.logLevel < level || !description) return "";
    util::Formatter f;
    String s;
    if (level == INFO)
        s += f.formatView("> %s\n", description);
    else
        s += f.formatView("[%-7s] %s\n", logs[level], description);
    return s;
}

static void handleParseStates(const char *description, DisplayLogLevel logLevel,
                              gram::LRParser const *lr) {
    util::Formatter f;
    String s;
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
    if (lr->hasMoreInput()) s += "...";

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
    String outputString;

    outputString += generateLogLine(description, logLevel);

    // Split symbols into terminals and non-terminals:
    // 1. Do not add epsilon.
    // 2. `$` must be included, but it's already in `symbols` so there's
    // no special care.
    std::vector<int> termVec, nontermVec;
    termVec.reserve(symbols.size());
    nontermVec.reserve(symbols.size());
    for (auto const &symbol : symbols) {
        if (symbol.type == gram::SymbolType::TERM && symbol.id != epsilonID)
            termVec.push_back(symbol.id);
        else if (symbol.type == gram::SymbolType::NON_TERM)
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
            String s = lr->dumpParseTableEntry(gram::StateID{i},
                                               gram::ActionID{terminal});
            outputString += f.formatView("|%*s ", actionWidth, s.c_str());
        }
        for (int nonterm : nontermVec) {
            String s = lr->dumpParseTableEntry(gram::StateID{i},
                                               gram::ActionID{nonterm});
            outputString += f.formatView("|%*s ", gotoWidth, s.c_str());
        }
        outputString += f.formatView("\n");
    }

    // No trailing '\n' is needed
    printf("%s", outputString.c_str());
}

// This function is slow
static void handleAutomaton(const char *description, DisplayLogLevel logLevel,
                            const char *prefix, Automaton const *automaton) {
    util::Formatter f;

    // Get file path
    fs::path outFile = launchArgs.resultsDir;
    outFile.append(f.formatView("%s.%d.gv", prefix, automatonCounter++));
    String gvFileName = outFile.string();

    handleLog(description, logLevel);

    String s = automaton->dump();

    FILE *file = std::fopen(gvFileName.c_str(), "w");
    std::fwrite(s.c_str(), sizeof(char), s.size(), file);
    std::fclose(file);

    outFile.replace_extension("svg");
    String svgFileName = outFile.string();

    if (!launchArgs.nodot) {
        auto clock = std::chrono::steady_clock();
        auto t1 = clock.now();

        futures.emplace_back(std::async(

            std::launch::async, [svgFileName = std::move(svgFileName),
                                 gvFileName = std::move(gvFileName)]() {
                auto clock = std::chrono::steady_clock();
                auto t1 = clock.now();

                std::array<const char *, 6> args = {"dot",
                                                    "-Tsvg",
                                                    "-o",
                                                    svgFileName.c_str(),
                                                    gvFileName.c_str(),
                                                    nullptr};
                proc::exec("dot", const_cast<char **>(&args[0]));

                auto t2 = clock.now();
                std::chrono::duration<double, std::milli> duration = t2 - t1;
                util::Formatter f;
                const char *description = f.formatView(
                                               "%.1f ms has been spent "
                                               "in creating a process",
                                               duration.count())
                                              .data();
                display(LOG, DEBUG, description);
            }));

        auto t2 = clock.now();
        std::chrono::duration<double, std::milli> duration = t2 - t1;
        util::Formatter f;
        const char *description = f.formatView(
                                       "%.1f ms has been spent "
                                       "in creating a thread",
                                       duration.count())
                                      .data();
        display(LOG, DEBUG, description);
    }
}

static void handleSymbolTable(const char *description, DisplayLogLevel logLevel,
                              gram::Grammar const *grammar) {
    constexpr const char *lineFmt = "%*s %10s %*s %16s\n";
    constexpr const int nameWidth = 10;
    constexpr const int firstWidth = 20;
    constexpr const char *dashline = "--------";

    auto &g = *grammar;
    util::Formatter f;
    String outputString;

    outputString += generateLogLine(description, logLevel);
    outputString += f.formatView(lineFmt, nameWidth, "Name", "Nullable",
                                 firstWidth, "First{}", "Follow{}");
    outputString += f.formatView(lineFmt, nameWidth, dashline, dashline,
                                 firstWidth, dashline, dashline);

    auto const &epsilon = g.getEpsilonSymbol();
    for (auto &symbol : g.getAllSymbols()) {
        // Do not output terminal information
        if (symbol.type == gram::SymbolType::TERM) {
            continue;
        }
        String nullable = g.dumpNullable(symbol);
        String firstSet = g.dumpFirstSet(symbol);
        String followSet = g.dumpFollowSet(symbol);
        int namew = (epsilon.id == symbol.id) ? nameWidth + 1 : nameWidth;
        int firstw =
            (firstSet.find(gram::Grammar::SignStrings::epsilon) != String::npos)
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
    String s = grammar->dump();
    String logLine = generateLogLine(description, logLevel);
    printf("%s%s\n", logLine.c_str(), s.c_str());
}

void display(DisplayType type, DisplayLogLevel level, const char *description,
             void const *pointer, void const *auxPointer) {
    switch (type) {
        case DisplayType::AUTOMATON:
            handleAutomaton(description, level, (const char *)auxPointer,
                            (Automaton const *)pointer);
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
            fprintf(stderr,
                    "[ERROR  ] Unknown display type. Check your code.\n");
            exit(1);
            break;
    }
}