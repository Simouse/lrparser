#ifndef LRPARSER_TOKEN_READER_H
#define LRPARSER_TOKEN_READER_H
#include <iostream>

#include "src/common.h"

namespace util {

struct TokenReader {
  virtual bool getToken(std::string &s) { return (bool)(stream >> s); }
  TokenReader(::std::istream &is) : stream(is) {}

 protected:
  ::std::istream &stream;
};

}  // namespace util

#endif