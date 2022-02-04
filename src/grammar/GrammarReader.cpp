//
// Created by "Simon Yu" on 2022/1/19.
//

#include "src/grammar/GrammarReader.h"

#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>

#include "src/common.h"
#include "src/grammar/Grammar.h"
#include "src/util/Formatter.h"

using std::ifstream;
using std::isblank;
using std::isspace;
using std::istream;

// Grammar Reader
namespace gram {

Grammar GrammarReader::parse(istream &stream) {
    Grammar g;
    GrammarReader reader(stream);
    reader.parse(g);
    return g;
}

void GrammarReader::parse(Grammar &g) try {
    std::string s;

    // Start T definition
    if (!launchArgs.autoDefineTerminals) {
        expectOrThrow("TERM");
        expectOrThrow(":");
        expectOrThrow("{");

        while (getToken(s)) {
            g.putSymbol(s.c_str(), true);
            if (!expect(','))
                break;
        }

        expectOrThrow("}");
    }

    // Start N definition
    bool startFound = false;

    while (getToken(s)) { // Has more rules
        auto nid = g.putSymbol(s.c_str(), false);
        if (!startFound) {
            g.setStart(s.c_str());
            startFound = true;
        }

        expectOrThrow("->");
        do {
            std::vector<SymbolID> productionBody;
            bool hasEpsilon = false;
            while (getToken(s, false)) {
                // put symbol and delay checking if auto-define is not enabled
                auto symid = launchArgs.autoDefineTerminals
                                 ? g.putSymbol(s.c_str(), true)
                                 : g.putSymbolUnchecked(s.c_str());
                productionBody.push_back(symid);
                if (symid == g.getEpsilonSymbol().id) {
                    hasEpsilon = true;
                }
            }
            if (productionBody.empty()) {
                throw std::runtime_error(
                    "No token found in right side of the rule. If you want to "
                    "use epsilon, use it explicitly");
            }
            if (hasEpsilon && productionBody.size() > 1) {
                throw std::runtime_error("Epsilon cannot be used along with "
                                         "other symbols in the same rule");
            }
            if (hasEpsilon) {
                productionBody.clear(); // Epsilon rule
            }
            g.addProduction(nid, std::move(productionBody));
        } while (expect('|'));
    }

    // Check redundant input (which normally means invalid syntax)
    const char *e = nullptr;
    if (!token.empty())
        e = token.c_str();
    if (skipSpaces(pos))
        e = pos;
    if (e)
        throw std::runtime_error(std::string("Redunant input: ") + e);

    g.checkViolations();

} catch (Grammar::UnsolvedSymbolError const &e) {
    std::string s = "Parsing error at line " +
                    std::to_string(tokenLineNo[e.symInQuestion.name]) + ": " +
                    e.what();
    fprintf(stderr, "%s\n", s.c_str());
    exit(1);
} catch (std::exception const &e) {
    std::string s = "Parsing error at line ";
    s += std::to_string(linenum);
    s += ", char ";
    auto offset = pos - lineStart + 1;
    if (offset > 0)
        s += std::to_string(offset);
    else
        s += "<Unknown>";
    s += ": ";
    s += e.what();
    fprintf(stderr, "%s\n", s.c_str());
    exit(1);
}

// Wrapper of getline
auto GrammarReader::getLineAndCount(istream &is, std::string &s) -> bool {
    if (std::getline(is, s)) {
        lineStart = s.c_str();
        ++linenum;
        return true;
    }
    return false;
}

static bool isCommentStart(char ch) { return ch == '!' || ch == '#'; }

// Make sure *p is non-space
// This is the only method to use `stream` directly, expect for
// getLineAndCount
auto GrammarReader::skipSpaces(const char *p) -> const char * {
    if (!p)
        return nullptr;
    while (true) {
        while (*p && isspace(*p))
            ++p;
        if (*p && !isCommentStart(*p))
            return p;
        // A comment start ('!') mark is equal to end of line
        // End of line: we should refetch string
        if (!getLineAndCount(stream, line))
            return nullptr;
        p = line.c_str();
    }
}

// Make sure *p is non-blank
// This is the only method to use `stream` directly, expect for
// getLineAndCount
auto GrammarReader::skipBlanks(const char *p) -> const char * {
    if (!p)
        return nullptr;
    // Do not fetch new line
    while (*p && isblank(*p))
        ++p;
    if (isCommentStart(*p)) {
        while (*p)
            ++p;
    }
    return p;
}

// Compare next non-space char with `ch`
auto GrammarReader::nextEquals(char ch) -> bool {
    if (!token.empty())
        return token[0] == ch;
    // No exceptions happen here, so we do not need to preserve pos
    pos = skipSpaces(pos);
    if (!pos)
        return false;
    return *pos == ch;
}

// Compare next non-space char with `ch`, and consume it if they are equal
auto GrammarReader::expect(char ch) -> bool {
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
auto GrammarReader::expectOrThrow(const char *expected) -> void {
    const char *expectedStart = expected;
    const char *check = skipSpaces(pos);

    if (!check) {
        std::string s = "Rules are incomplete: Expecting \"";
        s += expectedStart;
        s += "\"";
        throw std::runtime_error(s);
    }
    while (*check && *expected && *check == *expected) {
        ++check;
        ++expected;
    }
    if (*expected) { // expected string should be exhausted but is not
        std::string s = "Characters do not match: Expecting \"";
        s += expectedStart;
        s += "\"";
        throw std::runtime_error(s);
    }

    pos = check;
}

bool GrammarReader::getToken(std::string &s) try {
    return getToken(s, true);
} catch (std::runtime_error const &e) {
    display(LOG, ERR, e.what());
    exit(1);
}

// Try to get a token. This process may fail, so flag is returned in bool)
// This process is difficult because C++ does not have a Scanner
// like Java, and buffer needs to be managed by ourselves.
auto GrammarReader::getToken(std::string &s, bool newlineAutoFetch) -> bool {
    if (!token.empty()) {
        s = token;
        token.clear();
        return true;
    }

    util::Formatter f;

    const char *p = newlineAutoFetch ? skipSpaces(pos) : skipBlanks(pos);
    if (!p) {
        display(LOG, DEBUG, "getToken(): No more input");
        return false;
    }

    // Check the first character
    // There are 4 cases: backslash quote digit(invalid) _alpha
    if (std::isdigit(*p)) {
        throw std::runtime_error(
            "getToken(): The first character of a token cannot be a digit");
    }

    s.clear();

    if (*p == '\'' || *p == '\"') {
        char quoteChar = *p; // Quote
        const char *cur = p + 1;
        for (; *cur && *cur != quoteChar; ++cur) {
            continue;
        }

        // Change pos so error report is more precise.
        pos = cur;
        if (*cur != quoteChar) {
            throw std::runtime_error(
                f.formatView("getToken(): Cannot find matching quote pair %c",
                             quoteChar)
                    .data());
        }

        // [p, pos] ===> [" ... "]
        // Advance pos so we don't get overlapped scanning results
        s.append(p + 1, pos++);

        for (char ch : s) {
            if (std::isspace(ch)) {
                throw std::runtime_error(
                    "getToken(): token cannot contain spaces");
            }
        }

        tokenLineNo[s] = linenum;
        return true;
    }

    // Allow `\` at the beginning so escaping sequences can work
    if (*p == '\\')
        s += *p++;

    for (; *p && (std::isalnum(*p) || *p == '_'); ++p) {
        s += *p;
    }

    // Even if s is empty, we may have skipped some spaces,
    // which saves time for later work
    pos = p;

    if (!s.empty()) {
        tokenLineNo[s] = linenum;
        return true;
    }

    display(LOG, DEBUG, f.formatView("getToken() stops here: %s", pos).data());

    return false;
}

auto GrammarReader::ungetToken(const std::string &s) -> void {
    if (!token.empty()) {
        throw std::logic_error("Number of ungot tokens > 1");
    }
    token = s;
}
// void GrammarReader::setGetTokenVerbose(bool flag) {
//   getTokenVerboseFlag = flag;
// }

} // namespace gram