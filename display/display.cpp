#include "../include/display.h"
#include "../include/synbase.h"
#include "../include/synctx.h"
#include <string>
#include <vector>

using std::string;
using std::vector;

namespace lr {
auto Display::all(const lr::SyntaxContext *ctx) -> std::string {
    string s;
    auto &symVec = ctx->getSymVec();
    auto &rules = ctx->getRules();
    const char *typeStr[] = {"  Nonterminals:\n", "  Terminals:\n"};
    vector<int> classifiedSyms[2];

    for (auto &sym : symVec) {
        classifiedSyms[(int)sym.type].push_back(sym.id);
    }
    s += "Symbols:\n";
    for (int i = 1; i >= 0; --i) {
        s += typeStr[i];
        for (int symid : classifiedSyms[i]) {
            auto &sym = symVec[symid];
            s += "    { name: \"" + sym.name + "\", ";
            s += "id: " + std::to_string(sym.id) + " }\n";
        }
    }

    s += "Rules:\n";
    for (int i = 0; i < rules.size(); ++i) {
        auto &rule = rules[i];
        if (rule.empty())
            continue; // No rule for this symbol
        s += "  " + symVec[i].name + " ->";
        bool firstRule = true;
        for (auto &ruleItem: rule) {
            if (!firstRule) {
                s += "    |";
            }
            for (int ruleRhs: ruleItem) {
                s += ' ';
                s += symVec[ruleRhs].name;
            }
            s += '\n';
            firstRule = false;
        }
    }

    return s;
}
} // namespace lr