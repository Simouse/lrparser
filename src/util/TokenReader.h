#ifndef LRPARSER_TOKENREADER_H
#define LRPARSER_TOKENREADER_H
#include "src/common.h"
#include <iostream>

namespace util {

struct TokenReader {
    ::std::istream &stream;
    bool getToken(String &s) { return (bool)(stream >> s); }
    TokenReader(::std::istream &is) : stream(is) {}
};

} // namespace util

#endif