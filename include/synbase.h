#ifndef __SYNBASE_H__
#define __SYNBASE_H__

#include <string>

enum SymType {
    NONTERM = 0,
    TERM = 1, // For bool comparison
    UNCHECKED
};

struct Symbol {
    SymType type;
    int id;
    std::string name;
};

#endif