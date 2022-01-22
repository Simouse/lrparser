#include <string.h>

#include <iostream>

#include "src/common.h"
#include "src/grammar/Grammar.h"
#include "src/grammar/lr.h"

using namespace gram;

LaunchArguments launchArgs;

void printUsageAndExit() {
  fprintf(
      stderr,
      "Usage: lrparser -g<Grammar file> [-o<Result Dir>] [--nodot] [--strict]\n"
      "\n"
      "This program read possibly SLR grammar from <Grammar file>, takes test "
      "sequence\n"
      "from standard input stream, and store analysis results into <Result "
      "Dir>.\n"
      "\n"
      "Possible command: lrparser -g grammar.txt -o results\n"
      "Grammar file format:\n"
      "    1) Use ! or # to start a line comment.\n"
      "    2) \" or ' can be used to quote a symbol.\n"
      "    3) \"'\" and '\"' are okay, but there's no way to use them both in "
      "one symbol\n"
      "    e.g.\n"
      "    # Define terminals\n"
      "    TERM :{ID, '(', ')', '+', '*'}\n"
      "    # Define start symbol\n"
      "    START: exp\n"
      "    # Define productions\n"
      "    exp   -> exp '+' term  | term\n"
      "    term  -> term '*' fac  | fac\n"
      "    fac   -> ID            | \"(\" exp ')'\n"
      "-o       option: Specify output directory. Default: results\n"
      "--nodot  option: Disable svg automaton output. Use this when you don't "
      "have `dot`)\n"
      "--strict option: Input tokens must conform to C-variable name style.\n"
      "                 Without this option, tokens are space splitted.\n");
  exit(1);
}

void parseMain() {
  Grammar g = Grammar::fromFile(launchArgs.grammarFileName.c_str());
  SyntaxAnalysisSLR slr(g);
  slr.buildNFA();
  slr.buildDFA();
  slr.buildParseTables();
  slr.test(std::cin);
}

void processLaunchArgs(int argc, char **argv) {
  int i = 1;
  while (i < argc) {
    if (strncmp("-g", argv[i], 2) == 0) {
      if ((launchArgs.grammarFileName = argv[i++] + 2).empty()) {
        if (i >= argc) printUsageAndExit();
        launchArgs.grammarFileName = argv[i++];
      }
    } else if (strncmp("-o", argv[i], 2) == 0) {
      if ((launchArgs.resultsDir = argv[i++] + 2).empty()) {
        if (i >= argc) printUsageAndExit();
        launchArgs.resultsDir = argv[i++];
      }
    } else if (strcmp("--nodot", argv[i]) == 0) {
      launchArgs.nodot = true;
      ++i;
    } else if (strcmp("--strict", argv[i]) == 0) {
      launchArgs.strict = true;
      ++i;
    } else {
      printUsageAndExit();
    }
  }
  if (launchArgs.grammarFileName.empty()) {
    printUsageAndExit();
  }
}

int main(int argc, char **argv) {
  util::Process::prevent_zombie();
  processLaunchArgs(argc, argv);
  parseMain();
}