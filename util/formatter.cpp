#include "util/formatter.h"
#include <cstdarg>
#include <string>

namespace util {
template <class... Ts>
std::string Formatter::format(const char *fmt, Ts... args) {
    return static_cast<std::string>(bufferAfterFormat(fmt, args...));
}

const char *Formatter::bufferAfterFormat(const char *fmt, ...) {
    int sz;
    va_list ap;

    va_start(ap, fmt);
    // ap cannot be used for more than once,
    // unless we reinitialize it
    while ((sz = vsnprintf(buffer, maxsz, fmt, ap)) >= maxsz) {
        maxsz *= 2;
        if (maxsz < sz + 1)
            maxsz = sz + 2;
        delete[] buffer;
        buffer = new char[maxsz];
        va_end(ap);
        va_start(ap, fmt); // Reuse ap
    }
    va_end(ap);
    return buffer;
}
} // namespace util