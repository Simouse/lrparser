#include <cstring>
#include <iostream>

#include "src/common.h"
#include "src/grammar/Grammar.h"
#include "src/grammar/lr.h"

using namespace gram;

LaunchArguments launchArgs;

void printUsageAndExit() {
    // Procedures in deconstructors can be tricky.
    launchArgs.launchSuccess = false;
    printf(
        "This program reads possibly SLR grammar from <Grammar file>, takes "
        "test sequence from standard input stream, and stores analysis results "
        "into <Result Dir>.\n"
        "\n"
        "Usage: lrparser -g<Grammar file> [-o<Result Dir>] [--nodot] "
        "[--strict] [--debug] [-h|--help]\n"
        "\n"
        "Possible command: lrparser -g grammar.txt -o results\n"
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
        "productions are automatically defined as non-terminals.\n"
        "    e.g.\n"
        "    # Define terminals\n"
        "    TERM :{ID, '(', ')', '+', '*'}\n\n"
        "    # Define start symbol\n"
        "    START: exp\n\n"
        "    # Define productions\n"
        "    exp   -> exp '+' term  | term\n"
        "    term  -> term '*' fac  | fac\n"
        "    fac   -> ID            | \"(\" exp ')'\n"
        "-o        option: Specify output directory. (Default: \"results\").\n"
        "-g        option: Specify grammar file path. (Default: "
        "\"grammar.txt\")\n"
        "--nodot   option: Disable svg automaton output. Use this when you "
        "don't have `dot` in your `PATH`.\n"
        "--strict  option: Input token names must conform to rules of grammar "
        "file. Without this option, they are simply space-splitted.\n"
        "--debug   option: Set output level to DEBUG.\n"
        "-h|--help option: Output help message.\n");
    exit(0);
}

void lrMain() {
    Grammar g = Grammar::fromFile(launchArgs.grammarFileName.c_str());
    SyntaxAnalysisSLR slr(g);
    slr.buildNFA();
    slr.buildDFA();
    slr.buildParseTables();
    slr.test(std::cin);
}

void lrParseArgs(int argc, char **argv) {
    using std::strcmp;
    using std::strncmp;
    for (int i = 1; i < argc; ++i) {
        if (strncmp("-g", argv[i], 2) == 0) {
            if ((launchArgs.grammarFileName = argv[i++] + 2).empty()) {
                if (i >= argc) printUsageAndExit();
                launchArgs.grammarFileName = argv[i];
            }
        } else if (strncmp("-o", argv[i], 2) == 0) {
            if ((launchArgs.resultsDir = argv[i++] + 2).empty()) {
                if (i >= argc) printUsageAndExit();
                launchArgs.resultsDir = argv[i];
            }
        } else if (strcmp("--nodot", argv[i]) == 0) {
            launchArgs.nodot = true;
        } else if (strcmp("--strict", argv[i]) == 0) {
            launchArgs.strict = true;
        } else if (strcmp("--debug", argv[i]) == 0) {
            launchArgs.logLevel = DEBUG;
        } else if (strcmp("--help", argv[i]) == 0 ||
                   strcmp("-h", argv[i]) == 0) {
            printUsageAndExit();
        } else {
            printf("Error: Illegal arguments.\n\n");
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
}
