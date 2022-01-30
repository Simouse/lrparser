#include <cstring>
#include <iostream>
#include <string>

#include "src/common.h"
#include "src/grammar/Grammar.h"
#include "src/parser/LR0Parser.h"
#include "src/parser/LR1Parser.h"
#include "src/parser/LRParser.h"
#include "src/parser/SLRParser.h"
#include "src/util/Formatter.h"


using namespace gram;

LaunchArguments launchArgs;

void printUsageAndExit(bool printErrorLine = true) {
    // Procedures in deconstructors can be tricky.
    launchArgs.launchSuccess = false;
    if (printErrorLine) {
        printf("Error: Illegal arguments.\n\n");
    }
    printf(
        "This program reads possibly LR grammar from <Grammar file>, takes "
        "test sequence from standard input stream, and stores analysis results "
        "into <Result Dir>.\n"
        "\n"
        "Usage: lrparser -t<Type> -g<Grammar file> [-o<Result Dir>] [--nodot] "
        "[--strict] [--debug] [-h|--help] [-s]\n"
        "\n"
        "Possible command: lrparser -tslr -g grammar.txt -o results\n"
        "Grammar file:\n"
        "    1) Use `!` or `#` to start a line comment.\n"
        "    2) Token naming is based on C-style variable naming. Besides, "
        "`\\` can appear at the first character of token, and quoted symbols "
        "are supported so you can use some operators directly. (See the next "
        "rule)\n"
        "    2) `\"` or `'` can be used to quote a symbol, e.g. '+'. \"'\" and "
        "'\"' are okay, but there's no way to use them both in one symbol. "
        "Spaces in a quoted string are not allowed.\n"
        "    4) \\e, _e, and \\epsilon are reserved for epsilon. \n"
        "    5) You shouldn't use `$` in grammar file.\n"
        "    6) Define terminals, the start symbol, and productions as the "
        "following example shows. All symbols at the left hand side of "
        "productions are automatically defined as non-terminals. The first "
        "non-terminal symbol is defined as the start symbol.\n"
        "    e.g.\n"
        "    # Define terminals\n"
        "    TERM :{ID, '(', ')', '+', '*'}\n\n"
        "    # Define productions\n"
        "    exp   -> exp '+' term  | term\n"
        "    term  -> term '*' fac  | fac\n"
        "    fac   -> ID            | \"(\" exp ')'\n"
        "-t        option: Choose a parser type. Available: lr0, slr, lalr, "
        "lr1. (Default: slr)\n"
        "-o        option: Specify output directory. (Default: \"results\").\n"
        "-g        option: Specify grammar file path. (Default: "
        "\"grammar.txt\")\n"
        "--nodot   option: Disable svg automaton output. Use this when you "
        "don't have `dot` in your `PATH`.\n"
        "--strict  option: Input token names must conform to rules of grammar "
        "file. Without this option, they are simply space-splitted.\n"
        "--debug   option: Set output level to DEBUG.\n"
        "-h|--help option: Output help message.\n"
        "-s        option: Read <stdin> step by step. If you have to process a "
        "very large input file, you may need this option. But without this "
        "option the parser can provide better display for input queue.\n");
    exit(0);
}

void lrMain() {
    Grammar g = Grammar::fromFile(launchArgs.grammarFileName.c_str());
    reportTime("Grammar rules read");

    // Choose a parser
    LRParser *parser;
    LR0Parser lr0{g};
    SLRParser slr{g};
    LR1Parser lr1{g};
    switch (launchArgs.parserType) {
    case LR0:
        parser = &lr0;
        break;
    case SLR:
        parser = &slr;
        break;
    case LR1:
        parser = &lr1;
        break;
    default:
        fprintf(stderr, "Unknown parser type. Check your code\n");
        exit(1);
    }

    parser->buildNFA();
    reportTime("NFA built");
    parser->buildDFA();
    reportTime("DFA built");
    parser->buildParserTable();
    reportTime("Parse table built");
    parser->test(std::cin);
    reportTime("Test finished");
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
    for (int i = 1; i < argc; ++i) {
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
        } else if (strcmp("--nodot", argv[i]) == 0) {
            launchArgs.nodot = true;
        } else if (strcmp("--strict", argv[i]) == 0) {
            launchArgs.strict = true;
        } else if (strcmp("--debug", argv[i]) == 0) {
            launchArgs.logLevel = DEBUG;
        } else if (strcmp("--help", argv[i]) == 0 ||
                   strcmp("-h", argv[i]) == 0) {
            printUsageAndExit(false);
        } else if (strcmp("-s", argv[i]) == 0) {
            launchArgs.exhaustInput = false;
        } else {
            printUsageAndExit();
        }
    }
    if (launchArgs.grammarFileName.empty()) {
        printUsageAndExit();
    }
}

int main(int argc, char **argv) {
    lrInit();
    lrParseArgs(argc, argv);
    lrMain();
    lrCleanUp();
    reportTime("Clean up");
}
