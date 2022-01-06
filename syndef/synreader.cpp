#include "../include/synreader.h"
#include "../include/synctx.h"
#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using std::ifstream;
using std::istream;
using std::make_unique;
using std::string;
using std::unique_ptr;

namespace lr {
void SyntaxReader::parseRules(SyntaxContext &ctx) try {
    string s;

    if (parsed != 0) {
        throw std::runtime_error("Method parseRule() can only be called once");
    }

    int eid = ctx.putSym("_e", true);
    ctx.addAlias(eid, "\\e");
    ctx.addAlias(eid, "\\epsilon");

    expectOrThrow("TERM");
    expectOrThrow(":");
    expectOrThrow("{");

    // Start T definition
    while (getToken(s)) {
        ctx.putSym(s.c_str(), true);
        if (!expect(','))
            break;
    }

    expectOrThrow("}");
    expectOrThrow("!");

    // Start N definition
    while (getToken(s)) { // Has more rules
        int nid = ctx.putSym(s.c_str(), false);
        std::vector<std::vector<int>> rules;

        expectOrThrow("->");
        do {
            std::vector<int> ruleRhs;
            while (getToken(s, false)) {
                // put symbol and delay checking
                int symid = ctx.putSym(s.c_str());
                ruleRhs.push_back(symid);
            }
            if (ruleRhs.empty()) {
                throw std::runtime_error(
                    "No token found in right side of the rule");
            }
            rules.push_back(std::move(ruleRhs));
        } while (expect('|'));

        ctx.putRules(nid, std::move(rules));
    }

    // Check redundant input (which normally means invalid syntax)
    const char *e = nullptr;
    if (!token.empty())
        e = token.c_str();
    if (skipSpaces(pos))
        e = pos;
    if (e)
        throw std::runtime_error(string("Redunant input: ") + e);

    ctx.checkSymTypes();

    parsed = 1;
} catch (UnsolvedSymbolError &e) {
    string s = "Parsing error at line " +
               std::to_string(tokenLineNo[e.symInQuestion.name]) + ": " +
               e.what();
    throw std::runtime_error(s);
} catch (std::runtime_error &e) {
    string s = "Parsing error at line ";
    s += std::to_string(linenum);
    s += ", char ";
    auto offset = pos - lineStart + 1;
    if (offset > 0)
        s += std::to_string(offset);
    else
        s += "<Unknown>";
    s += ": ";
    s += e.what();
    throw std::runtime_error(s);
}

// Wrapper of getline
auto SyntaxReader::getlineAndCount(istream &is, string &s) -> bool {
    if (std::getline(is, s)) {
        lineStart = s.c_str();
        ++linenum;
        return true;
    }
    return false;
}

// Make sure *p is non-space
// This is the only method to use `stream` directly, expect for
// getlineAndCount
auto SyntaxReader::skipSpaces(const char *p) -> const char * {
    if (!p)
        return nullptr;
    while (true) {
        while (*p && isspace(*p))
            ++p;
        if (*p)
            return p;
        // End of line: we should refetch string
        if (!getlineAndCount(stream, line))
            return nullptr;
        p = line.c_str();
    }
    throw std::logic_error("Unreachable code");
}

// Make sure *p is non-blank
// This is the only method to use `stream` directly, expect for
// getlineAndCount
auto SyntaxReader::skipBlanks(const char *p) -> const char * {
    if (!p)
        return nullptr;
    // Do not fetch new line
    while (*p && std::isblank(*p))
        ++p;
    return p;
}

// Compare next non-space char with `ch`
auto SyntaxReader::nextEquals(char ch) -> bool {
    if (!token.empty())
        return token[0] == ch;
    // No exceptions happen here so we do not need to preserve pos
    pos = skipSpaces(pos);
    if (!pos)
        return false;
    return *pos == ch;
}

// Compare next non-space char with `ch`, and consume it if they are equal
auto SyntaxReader::expect(char ch) -> bool {
    if (!token.empty()) {
        if (token[0] == ch) {
            token = token.substr(1);
            return true;
        }
        return false;
    }
    pos = skipSpaces(pos);
    if (!pos || *pos != ch)
        return false;
    ++pos;
    return true;
}

// Compare next non-space sequence with `expected`,
// and consume it if they are equal. Otherwise, an exception is thrown.
auto SyntaxReader::expectOrThrow(const char *expected) -> void {
    const char *expectedStart = expected;
    const char *check = skipSpaces(pos);

    if (!check) {
        string s = "Rules are incomplete: Expecting \"";
        s += expectedStart;
        s += "\"";
        throw std::runtime_error(s);
    }
    while (*check && *expected && *check == *expected) {
        ++check;
        ++expected;
    }
    if (*expected) { // expected string should be exhausted but is not
        string s = "Characters do not match: Expecting \"";
        s += expectedStart;
        s += "\"";
        throw std::runtime_error(s);
    }

    pos = check;
}

// TODO: take care of special cases: \e.
// Try to get a token (This process may fail).
// This process is diffcult because C++ does not have a Scanner
// like Java, and buffer needs to be managed by ourselves.
auto SyntaxReader::getToken(string &s, bool multiline) -> bool {
    if (!token.empty()) {
        s = token;
        token.clear();
        return true;
    }

    const char *p = multiline ? skipSpaces(pos) : skipBlanks(pos);
    if (!p)
        return false;
    if (std::isdigit(*p)) {
        throw std::runtime_error(
            "The first character of a token cannot be a digit");
    }
    s.clear();
    for (; *p && (std::isalnum(*p) || *p == '_'); ++p) {
        s += *p;
    }
    pos = p;
    if (!s.empty()) {
        tokenLineNo[s] = linenum;
        return true;
    }
    return false;
}

auto SyntaxReader::ungetToken(const string &s) -> void {
    if (!token.empty()) {
        throw std::logic_error("Number of ungot tokens > 1");
    }
    token = s;
}

} // namespace lr