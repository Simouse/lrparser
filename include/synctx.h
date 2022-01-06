#ifndef __SYNCTX_H__
#define __SYNCTX_H__

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "synbase.h"

namespace lr {
class UnsolvedSymbolError : public std::runtime_error {
  public:
    UnsolvedSymbolError(const Symbol &sym)
        : symInQuestion(sym), std::runtime_error("Unsolved symbol: " + sym.name) {}
    // [[nodiscard]] const char* what() const noexcept override;
    const Symbol &symInQuestion;
};

class SyntaxContext {
  private:
    std::vector<std::vector<std::vector<int>>> rules;
    std::vector<Symbol> symVec;
    std::unordered_map<std::string, int> idTable;
    std::vector<int> uncheckedSyms;
    SyntaxContext() = default;
    auto putSym(Symbol &&sym) -> int; // Detect duplicates

  public:
    auto putSym(const char *name, bool isTerm) -> int;
    auto putSym(const char *name) -> int; // Delay checking
    void addAlias(int sid, const char *alias);
    void putRules(int nid, std::vector<std::vector<int>> &&rules);
    void checkSymTypes(); // Check if there's a sym with no type
    [[nodiscard]] auto findSym(int id) const -> const Symbol &;
    [[nodiscard]] auto findSym(const char *s) const -> const Symbol &;
    [[nodiscard]] auto findSym(const char *s, bool isTerm) const
        -> const Symbol &;

    // Do not throw:
    [[nodiscard]] auto getSym(int id) const -> const Symbol &;
    [[nodiscard]] auto getSymVec() const -> const decltype(symVec) &;
    [[nodiscard]] auto getRules() const -> const decltype(rules) &;

    // Factories:
    static auto fromFile(const char *fileName) -> SyntaxContext *;
    static auto fromStdin() -> SyntaxContext *;
};
} // namespace lr

#endif