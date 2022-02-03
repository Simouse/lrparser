//
// Created by "Simon Yu" on 2022/1/19.
//

#ifndef LRPARSER_READER_H
#define LRPARSER_READER_H

#include "src/common.h"
#include "src/util/TokenReader.h"
#include <unordered_map>

namespace gram {
class Grammar;
}

namespace gram {
// TODO: check if unnecessary terminals exist
class GrammarReader: public util::TokenReader {
  private:
    // bool getTokenVerboseFlag = false;
    int linenum = 0;
    // Make sure pos is empty but not NULL in the beginning
    const char *pos = "";
    // Only valid if pos != NULL and stream is read at least once
    const char *lineStart = nullptr;
    std::string line;
    // `token` is used by ungetToken()
    std::string token;
    std::unordered_map<std::string, int> tokenLineNo;

    auto getLineAndCount(std::istream &is, std::string &s) -> bool;
    auto skipSpaces(const char *p) -> const char *;
    static auto skipBlanks(const char *p) -> const char *;

  public:
    static Grammar parse(std::istream &stream);
    explicit GrammarReader(std::istream &is) : util::TokenReader(is) {}
    bool getToken(std::string &s, bool newlineAutoFetch);
    bool getToken(std::string &s) override;
    void ungetToken(const std::string &s);
    void parse(Grammar &g);
    auto nextEquals(char ch) -> bool;
    auto expect(char ch) -> bool;
    void expectOrThrow(const char *expected);
    // void setGetTokenVerbose(bool flag);
};
} // namespace gram

#endif // LRPARSER_READER_H
