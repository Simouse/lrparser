//
// Created by "Simon Yu" on 2022/1/19.
//

#ifndef LRPARSER_READER_H
#define LRPARSER_READER_H

#include "src/common.h"
#include <unordered_map>

namespace gram {
class Grammar;
}

namespace gram {
// TODO: check if unnecessary terminals exist
class GrammarReader {
  private:
    bool getTokenVerboseFlag = false;
    int linenum = 0;
    // Make sure pos is empty but not NULL in the beginning
    const char *pos = "";
    // Only valid if pos != NULL and stream is read at least once
    const char *lineStart = nullptr;
    String line;
    // `token` is used by ungetToken()
    String token;
    std::istream &stream;
    std::unordered_map<String, int> tokenLineNo;

    auto getLineAndCount(std::istream &is, String &s) -> bool;
    auto skipSpaces(const char *p) -> const char *;
    static auto skipBlanks(const char *p) -> const char *;

  public:
    static Grammar parse(std::istream &stream);
    explicit GrammarReader(std::istream &is) : stream(is) {}
    bool getToken(String &s, bool newlineAutoFetch = true);
    void ungetToken(const String &s);
    void parse(Grammar &g);
    auto nextEquals(char ch) -> bool;
    auto expect(char ch) -> bool;
    void expectOrThrow(const char *expected);
    void setGetTokenVerbose(bool flag);
};
} // namespace gram

#endif // LRPARSER_READER_H
