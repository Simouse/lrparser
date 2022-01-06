#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <string>

namespace lr {
class SyntaxContext;

struct Display {
    static std::string all(const lr::SyntaxContext *ctx);
};
} // namespace lr

#endif