#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "src/automata/automata.h"
#include "src/common.h"
#include "src/grammar/grammar.h"
#include "src/grammar/syntax.h"
#include "src/util/exec.h"

namespace fs = std::filesystem;
using proc = util::Process;

struct DefaultDisplay {
    void operator()(DisplayType type, const char *description, void *data) {
        switch (type) {
        case DisplayType::AUTOMATON:
            handleAutomaton(description,
                            static_cast<gram::Automaton const *>(data));
            break;
        case DisplayType::SYMBOL_TABLE:
            handleSymbolTable(description,
                              static_cast<gram::Grammar const *>(data));
            break;
        case DisplayType::LOG:
            handleLog(description);
            break;
        case DisplayType::PARSE_TABLE:
            handleParseTable(description,
                             static_cast<gram::SyntaxAnalysisLR const *>(data));
            break;
        default:
            fprintf(stderr, "[WARN] Unknown display type. Check your code.\n");
            break;
        }
    }

  private:
    void handleLog(const char *description) { printf("> %s\n", description); }

    void handleParseTable(const char *description,
                          gram::SyntaxAnalysisLR const *lr) {
        constexpr const int indexWidth = 8;
        constexpr const int actionWidth = 8;
        constexpr const int gotoWidth = 6;
        constexpr const char *dashline = "--------";

        auto const &actionTable = lr->getActionTable();
        auto const &grammar = lr->getGrammer();
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
        for (int i = 0; i < dfaStates.size(); ++i) {
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

        fflush(stdout);
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
        fflush(stdout);
        // std::this_thread::sleep_for(std::chrono::milliseconds(15));
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
                          firstSet.npos)
                             ? firstWidth + 1
                             : firstWidth;
            printf(lineFmt, namew, symbol.name.c_str(), nullable.c_str(),
                   firstw, firstSet.c_str(), followSet.c_str());
        }
        fflush(stdout);
    }
};

DisplayCallable display = DefaultDisplay{};

namespace gram {

/*

void FileOutputDisplay::show(const gram::Automaton &m, StringView description) {
    fs::path outFile = outDir;
    String automatonType = m.isDFA() ? "dfa" : "nfa";
    outFile /= (automatonType + fileSuffix);
    String outFileName = outFile.string();

    const char *args[] = {
        "dot", "-T", &fileSuffix[1], "-o", outFileName.c_str(), nullptr};
    proc::Stream stream = proc::execw("dot", const_cast<char **>(args));

    String s = m.dump();  // s should already have a '\n'
    proc::writeStream(stream, "%s", s.c_str());
    proc::closeStream(stream);

    std::cout << "> " << description << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
}

FileOutputDisplay::FileOutputDisplay(const char *dir, long long int delayInMs,
                                     FileOutputDisplay::FileType ft)
    : outDir(dir), delay(delayInMs) {
    switch (ft) {
        case FileType::SVG:
            fileSuffix = ".svg";
            break;
        case FileType::PDF:
            fileSuffix = ".pdf";
            break;
        case FileType::PNG:
            fileSuffix = ".png";
            break;
        default:
            fileSuffix = nullptr;
    }
}

void ConsoleDisplay::show(const gram::Automaton &m, StringView description) {
    std::cout << "> " << description << '\n';
    std::cout << m.dump() << std::flush;
}

void ConsoleDisplay::showSymbols(const gram::Grammar &g,
                                 StringView description) {
    constexpr const char *lineFmt = "%*s %10s %*s %16s\n";
    constexpr const int nameWidth = 10;
    constexpr const int firstWidth = 20;

    std::cout << "> " << description << '\n';
    const char *dashline = "--------";
    std::printf(lineFmt, nameWidth, "Name", "Nullable", firstWidth, "First{}",
                "Follow{}");
    std::printf(lineFmt, nameWidth, dashline, dashline, firstWidth, dashline,
                dashline);

    auto const &epsilon = g.getEpsilonSymbol();
    for (auto &symbol : g.getAllSymbols()) {
        // Do not output terminal information
        if (symbol.type == SymbolType::TERM) {
            continue;
        }
        String nullable = g.dumpNullable(symbol);
        String firstSet = g.dumpFirstSet(symbol);
        String followSet = g.dumpFollowSet(symbol);
        int namew = (epsilon.id == symbol.id) ? nameWidth + 1 : nameWidth;
        int firstw = (firstSet.find(Grammar::SignStrings::epsilon) !=
firstSet.npos) ? firstWidth + 1 : firstWidth; std::printf(lineFmt, namew,
symbol.name.c_str(), nullable.c_str(), firstw, firstSet.c_str(),
followSet.c_str());
    }
    std::cout << std::flush;
}

*/

} // namespace gram