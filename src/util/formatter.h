#ifndef LRPARSER_FORMATTER_H
#define LRPARSER_FORMATTER_H

#include <cstdarg>
#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>


#include "src/common.h"

namespace util {

class Formatter {
    static constexpr int INIT_SIZE = 128;
    char mbuf[INIT_SIZE];
    char *buf;
    int maxSize = INIT_SIZE;

  public:
    Formatter() : buf(mbuf) { mbuf[0] = '\0'; }
    Formatter(Formatter const &f) = delete;

    // Return value should be used right away
    template <class... Ts>
    std::string_view formatView(const char *fmt, Ts const &...args) {
        static_assert(((std::is_scalar_v<Ts> || std::is_array_v<Ts>)&&...),
                      "All arguments must be of primitive types");

        int sz;

        while ((sz = ::std::snprintf(buf, maxSize, fmt, args...)) >= maxSize) {
            maxSize *= 2;
            if (maxSize < sz + 1) {
                maxSize = sz + 2;
            }
            if (buf != mbuf) {
                delete[] buf;
            }
            buf = new char[maxSize];
        }

        return {buf, static_cast<size_t>(sz)};
    }

    template <class... Ts>
    std::string format(const char *fmt, Ts const &...args) {
        auto sv = formatView(fmt, args...);
        return {sv.data(), sv.size()};
    }

    // This method protects chars from being escaped.
    // e.g. \" ===> \\\" so when you pass this string to functions
    // like printf, it's not interpreted as an escape sequence.
    [[nodiscard]] static std::string reverseEscape(std::string_view sv) {
        std::string s;
        for (char ch : sv) {
            switch (ch) {
            case '\'':
            case '\"':
            case '\\':
                s += '\\';
                s += ch;
                break;
            case '\n':
                s += "\\n";
                break;
            case '\t':
                s += "\\t";
                break;
            default:
                s += ch;
            }
        }
        return s;
    }

    // Concat executable path and thoses arguments used by command line.
    // The 0th argument is ignored, and there should be a trailing NULL pointer.
    // NULL as an argument is not checked.
    [[nodiscard]] static std::string concatArgs(const char *path, char **ptr) {
        std::string s = path;
        bool firstArgSkipped = false;
        for (; *ptr; ++ptr) {
            if (firstArgSkipped) {
                s += ' ';
                s += *ptr;
            }
            firstArgSkipped = true;
        }
        return s;
    }

    ~Formatter() {
        if (buf != mbuf)
            delete[] buf;
    }
};
} // namespace util

#endif