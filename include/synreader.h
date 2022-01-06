#ifndef __SYNREADER_H__
#define __SYNREADER_H__

#include <istream>
#include <string>
#include <unordered_map>

namespace lr {
class SyntaxContext;

// TODO: add \e support
// TODO: add comment support
// TODO: add 'c' support
// TODO: add "str" support
// TODO: check if all terminals in rule can be found
// TODO: check if a symbol is redefined but types are different
// TODO: check if unnecessary terminals exist
// TODO: convert fstream to universe istream

class SyntaxReader {
  private:
    int linenum = 0, parsed = 0;
    // Make sure pos is empty but not NULL in the beginning
    const char *pos = ""; 
    // Only valid if pos != NULL and stream is read at least once
    const char *lineStart;
    std::string line, token;
    std::istream &stream;
    std::unordered_map<std::string, int> tokenLineNo;

    auto getlineAndCount(std::istream &is, std::string &s) -> bool;
    auto skipSpaces(const char *p) -> const char *;
    auto skipBlanks(const char *p) -> const char *;
    auto nextEquals(char ch) -> bool;
    auto expect(char ch) -> bool;
    void expectOrThrow(const char *expected);
    auto getToken(std::string &s, bool multiline = true) -> bool;
    void ungetToken(const std::string &s);

  public:
    void parseRules(SyntaxContext &ctx);
    SyntaxReader(std::istream &is): stream(is) {}
};
} // namespace lr

#endif