#include "./include/display.h"
#include "./include/synctx.h"
#include "./include/synreader.h"
#include <exception>
#include <iostream>
#include <vector>

using std::vector;
using namespace lr;

int main() {
    try {
        SyntaxContext *ctx = SyntaxContext::fromStdin();
        std::cout << Display::all(ctx);
    } catch (std::exception &e) {
        std::cerr << "[Error] " << e.what() << '\n';
    }
}