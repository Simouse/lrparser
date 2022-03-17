#include <cctype>
#include <cstring>
#include <exception>
#include <iostream>
#include <cstdlib>
#include <string>

#include "src/common.h"
#include "src/grammar/Grammar.h"
#include "src/parser/LALRParser.h"
#include "src/parser/LR0Parser.h"
#include "src/parser/LR1Parser.h"
#include "src/parser/LRParser.h"
#include "src/parser/SLRParser.h"
#include "src/util/Formatter.h"
#include "src/display/steps.h"

using namespace gram;

LaunchArguments launchArgs;

extern const char *help_message; // in help.txt

void printUsageAndExit(bool onError = true) {
    launchArgs.launchSuccess = false;
    if (onError) {
        fprintf(stderr, "Error: Illegal arguments.\n\n");
        fprintf(stderr, "%s", help_message);
    } else {
        fprintf(stdout, "%s", help_message);
    }
    exit(0);
}

void lrMain() {
    Grammar g = Grammar::fromFile(launchArgs.grammarFileName.c_str());
    reportTime("Grammar rules read");

    // Choose a parser
    LRParser *parser = nullptr;
    ParserType t = launchArgs.parserType;

    if (t == LR0)       parser = new LR0Parser(g);
    else if (t == SLR)  parser = new SLRParser(g);
    else if (t == LR1)  parser = new LR1Parser(g);
    else if (t == LALR) parser = new LALRParser(g);
    else {
        fprintf(stderr, "Unknown parser type. Check your code\n");
        exit(1);
    }

    parser->buildNFA();
    reportTime("NFA built");

    step::section("DFA");
    parser->buildDFA();
    reportTime("DFA built");
    // step::show("DFA is built.");
    
    step::section("Parse Table");
    parser->buildParseTable();
    reportTime("Parse table built");

    if (!launchArgs.noTest) {
        parser->test(std::cin);
        reportTime("Test finished");
    }

    delete parser;
}

void chooseParserType(const char *s) {
    if (strcmp("lr0", s) == 0) {
        launchArgs.parserType = LR0;
    } else if (strcmp("slr", s) == 0) {
        launchArgs.parserType = SLR;
    } else if (strcmp("lalr", s) == 0) {
        launchArgs.parserType = LALR;
    } else if (strcmp("lr1", s) == 0) {
        launchArgs.parserType = LR1;
    } else {
        printUsageAndExit();
    }
}

void lrParseArgs(int argc, char **argv) {
    using std::strcmp;
    using std::strncmp;
    for (int i = 0; i < argc; ++i) {
        if (strncmp("-g", argv[i], 2) == 0) {
            if ((launchArgs.grammarFileName = argv[i] + 2).empty()) {
                if (++i >= argc)
                    printUsageAndExit();
                launchArgs.grammarFileName = argv[i];
            }
        } else if (strncmp("-o", argv[i], 2) == 0) {
            if ((launchArgs.resultsDir = argv[i] + 2).empty()) {
                if (++i >= argc)
                    printUsageAndExit();
                launchArgs.resultsDir = argv[i];
            }
        } else if (strncmp("-t", argv[i], 2) == 0) {
            if (*(argv[i] + 2)) {
                chooseParserType(argv[i] + 2);
            } else {
                if (++i >= argc)
                    printUsageAndExit();
                chooseParserType(argv[i]);
            }
        } else if (strcmp("--strict", argv[i]) == 0) {
            launchArgs.strict = true;
        } else if (strcmp("--debug", argv[i]) == 0) {
            launchArgs.logLevel = DEBUG;
        } else if (strcmp("--help", argv[i]) == 0 ||
                   strcmp("-h", argv[i]) == 0) {
            printUsageAndExit(false);
        } else if (strcmp("--step", argv[i]) == 0) {
            launchArgs.exhaustInput = false;
        } else if (strcmp("--no-test", argv[i]) == 0) {
            launchArgs.noTest = true;
        } else if (strncmp("--sep=", argv[i], 13) == 0) {
            launchArgs.sep = argv[i] + 13;
            // Check if arg is space
            auto const &s = launchArgs.sep;
            bool hasSpace = s.empty();
            for (char ch : s) {
                if (std::isspace(ch)) {
                    hasSpace = true;
                    break;
                }
            }
            if (hasSpace) {
                fprintf(stderr, "Error: Argument \"--sep=\" does not "
                                "have a valid value.\n");
                printUsageAndExit();
            }
        } else if (strcmp("--no-label", argv[i]) == 0) {
            launchArgs.noPDALabel = true;
        } else {
            printUsageAndExit();
        }
    }
    if (launchArgs.grammarFileName.empty()) {
        printUsageAndExit();
    }
}

int main(int argc, char **argv) try {
    lrParseArgs(argc - 1, argv + 1);
    lrInit();
    lrMain();
    lrCleanUp();
    reportTime("Clean up");
} catch (std::exception &e) {
    fprintf(stderr, "%s\n", e.what());
    exit(1);
}
