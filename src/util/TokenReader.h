#ifndef LRPARSER_TOKENREADER_H
#define LRPARSER_TOKENREADER_H
#include <iostream>

#include "src/common.h"

namespace util {

struct TokenReader {
  virtual bool getToken(String &s) { return (bool)(stream >> s); }
  TokenReader(::std::istream &is) : stream(is) {}

 protected:
  ::std::istream &stream;
};

}  // namespace util

#endif