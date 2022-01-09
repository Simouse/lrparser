#ifndef __FORMATTER_H__
#define __FORMATTER_H__

#include <string>

namespace util {
class Formatter {
    static constexpr int BUFSIZE = 256;
    char *buffer = new char[BUFSIZE];
    int maxsz = BUFSIZE;

  public:
    // Return value should be used right away
    const char *bufferAfterFormat(const char *fmt, ...);

    template <class... Ts> std::string format(const char *fmt, Ts... args);

    ~Formatter() { delete[] buffer; }
};
} // namespace util

#endif