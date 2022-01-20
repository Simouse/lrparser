#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "src/automata/Automaton.h"
#include "src/common.h"
#include "src/grammar/Grammar.h"
#include "src/grammar/lr.h"
#include "src/util/Process.h"

namespace fs = std::filesystem;
using namespace gram;
using proc = util::Process;

struct DefaultDisplay {
    void operator()(DisplayType type, DisplayData data,
                    const char *description) {
        switch (type) {
        case DisplayType::AUTOMATON:
            handleAutomaton(description, (Automaton const *)data.pointer);
            break;
        case DisplayType::SYMBOL_TABLE:
            handleSymbolTable(description, (Grammar const *)data.pointer);
            break;
        case DisplayType::LOG:
            handleLog(description, (DisplayLogLevel)data.logLevel);
            break;
        case DisplayType::PARSE_TABLE:
            handleParseTable(description,
                             (SyntaxAnalysisLR const *)data.pointer);
            break;
        case DisplayType::GRAMMAR_RULES:
            handleGrammarRules(description, (Grammar const *)data.pointer);
            break;
        case DisplayType::LR_STATE_STACK:
            handleStateStack(description,
                             (std::vector<StateID> const *)data.pointer);
            break;
        case DisplayType::LR_SYMBOL_STACK:
            handleSymbolStack(description,
                              (SymbolStackInfo const *)data.pointer);
            break;
        default:
            fprintf(stderr, "[WARN] Unknown display type. Check your code.\n");
            break;
        }
    }

    DefaultDisplay() {
        logs[DisplayLogLevel::INFO] = "INFO";
        logs[DisplayLogLevel::VERBOSE] = "VERBOSE";
        logs[DisplayLogLevel::DEBUG] = "DEBUG";
        logs[DisplayLogLevel::ERR] = "ERROR";
    }

  private:
    std::array<const char *, DisplayLogLevel::LOG_LEVELS_COUNT> logs = {0};

    void handleLog(const char *description, DisplayLogLevel level) {
        if (level == DisplayLogLevel::INFO)
            printf("> %s\n", description);
        else
            printf("[%-7s] %s\n", logs[level], description);
    }

    void handleSymbolStack(const char *description,
                           SymbolStackInfo const *info) {
        printf("> %-14s: Bottom| ", description);
        size_t sz = info->symbolStack->size();
        auto const &symbols = info->grammar->getAllSymbols();
        for (size_t i = 0; i < sz; ++i) {
            printf(i == 0 ? "%s" : ",%s",
                   symbols[info->symbolStack->operator[](i)].name.c_str());
        }
        printf("\n");
        // fflush(stdout);
    }

    void handleStateStack(const char *description,
                          std::vector<gram::StateID> const *stack) {
        printf("> %-14s: Bottom| ", description);
        size_t sz = stack->size();
        for (size_t i = 0; i < sz; ++i) {
            printf(i == 0 ? "%d" : ",%d", stack->operator[](i));
        }
        printf("\n");
        // fflush(stdout);
    }

    void handleParseTable(const char *description,
                          gram::SyntaxAnalysisLR const *lr) {
        constexpr const int indexWidth = 8;
        constexpr const int actionWidth = 8;
        constexpr const int gotoWidth = 6;
        constexpr const char *dashline = "--------";

        auto const &actionTable = lr->getActionTable();
        auto const &grammar = lr->getGrammar();
        auto const &symbols = grammar.getAllSymbols();
        auto const &dfa = lr->getDFA();
        auto const &dfaStates = dfa.getAllStates();
        auto epsilonID = grammar.getEpsilonSymbol().id;

        printf("> %s\n", description);

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
        printf("%*s %*s %*s\n", indexWidth, "States", allActionsWidth,
               "Actions", allGotosWidth, "Gotos");
        printf("%*s %*s %*s\n", indexWidth, dashline, allActionsWidth, dashline,
               allGotosWidth, dashline);

        // Print symbol names
        printf("%*s ", indexWidth, "");
        for (int terminal : termVec) {
            printf("|%*s ", actionWidth, symbols[terminal].name.c_str());
        }
        for (int nonterm : nontermVec) {
            printf("|%*s ", gotoWidth, symbols[nonterm].name.c_str());
        }
        printf("\n");

        // Print table entries
        for (int i = 0; static_cast<size_t>(i) < dfaStates.size(); ++i) {
            printf("%*d ", indexWidth, i);
            for (int terminal : termVec) {
                String s = lr->dumpParseTableEntry(gram::StateID{i},
                                                   gram::ActionID{terminal});
                printf("|%*s ", actionWidth, s.c_str());
            }
            for (int nonterm : nontermVec) {
                String s = lr->dumpParseTableEntry(gram::StateID{i},
                                                   gram::ActionID{nonterm});
                printf("|%*s ", gotoWidth, s.c_str());
            }
            printf("\n");
        }

        // fflush(stdout);
    }

    void handleAutomaton(const char *description,
                         gram::Automaton const *automaton) {
        constexpr const char *fileSuffix = ".svg";

        fs::path outFile = ".";
        String automatonType = automaton->isDFA() ? "dfa" : "nfa";
        outFile /= (automatonType + fileSuffix);
        String outFileName = outFile.string();

        const char *args[] = {
            "dot", "-T", &fileSuffix[1], "-o", outFileName.c_str(), nullptr};
        proc::Stream stream = proc::execw("dot", const_cast<char **>(args));

        String s = automaton->dump(); // s should already have a '\n'
        proc::writeStream(stream, "%s", s.c_str());
        proc::closeStream(stream);

        printf("> %s\n", description);
        // fflush(stdout);
    }

    void handleSymbolTable(const char *description,
                           gram::Grammar const *grammar) {
        constexpr const char *lineFmt = "%*s %10s %*s %16s\n";
        constexpr const int nameWidth = 10;
        constexpr const int firstWidth = 20;
        constexpr const char *dashline = "--------";

        auto &g = *grammar;

        printf("> %s\n", description);
        printf(lineFmt, nameWidth, "Name", "Nullable", firstWidth, "First{}",
               "Follow{}");
        printf(lineFmt, nameWidth, dashline, dashline, firstWidth, dashline,
               dashline);

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
            int firstw = (firstSet.find(gram::Grammar::SignStrings::epsilon) !=
                          String::npos)
                             ? firstWidth + 1
                             : firstWidth;
            printf(lineFmt, namew, symbol.name.c_str(), nullable.c_str(),
                   firstw, firstSet.c_str(), followSet.c_str());
        }
        // fflush(stdout);
    }

    void handleGrammarRules(const char *description,
                            gram::Grammar const *grammar) {
        String s = grammar->dump();
        printf("> %s\n%s", description, s.c_str());
        // fflush(stdout);
    }
};

DisplayCallable display = DefaultDisplay{};
