#include "src/common.h"
#include "src/display/steps.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdarg.h>
#include <stdio.h>
#include <string>


extern FILE *stepFile;

static const char *bool_str[2] = {"False", "True"};

namespace step {

void symbol(int id, const char *name, bool is_term, bool is_start) {
    std::string escaped_name;
    escape_ascii(name, std::back_inserter(escaped_name), '\'');
    fprintf(stepFile, "symbol[%d].name=%s\n", id, escaped_name.c_str());
    fprintf(stepFile, "symbol[%d].is_term=%s\n", id, bool_str[is_term]);
    fprintf(stepFile, "symbol[%d].is_start=%s\n", id, bool_str[is_start]);
}

void production(int id, int head, const int *body, size_t body_size) {
    fprintf(stepFile, "production[%d].head = %d\n", id, head);
    fprintf(stepFile, "production[%d].body = [", id);
    for (size_t index = 0; index < body_size; ++index) {
        fprintf(stepFile, index ? ", %d" : "%d", body[index]);
    }
    fprintf(stepFile, "]\n");
    fprintf(stepFile, "symbol[%d].productions.append(%d)\n", head, id);
}

void nullable(int symbol, bool nullable, const char *explain) {
    fprintf(stepFile, "symbol[%d].nullable = %-5s\n", symbol,
            bool_str[nullable]);
    show(explain);
}

void addFirst(int symbol, int component, const char *explain) {
    fprintf(stepFile, "symbol[%d].first.add(%d)\n", symbol, component);
    show(explain);
}

void addFollow(int symbol, int component, const char *explain) {
    fprintf(stepFile, "symbol[%d].follow.add(%d)\n", symbol, component);
    show(explain);
}

void mergeFollow(int dest, int src, const char *explain) {
    fprintf(stepFile, "symbol[%d].follow.update(symbol[%d].follow)\n", dest,
            src);
    show(explain);
}

void addTableEntry(int state, int look_ahead, const char *action) {
    fprintf(stepFile, "table[%d][%d].add('%s')\n", state, look_ahead, action);
}

void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stepFile, fmt, ap);
}

void addState(int state, std::string_view description) {
    std::string s;
    escape_ascii(description, std::back_inserter(s), '\"');
    fprintf(stepFile, "addState(%d, %s)\n", state, s.c_str());
}

void updateState(int state, std::string_view description) {
    std::string s;
    escape_ascii(description, std::back_inserter(s), '\"');
    fprintf(stepFile, "updateState(%d, %s)\n", state, s.c_str());
}

void addEdge(int s1, int s2, std::string_view label) {
    std::string s;
    escape_ascii(label, std::back_inserter(s), '\"');
    fprintf(stepFile, "addEdge(%d, %d, %s)\n", s1, s2, s.c_str());
}

void setStart(int state) { fprintf(stepFile, "setStart(%d)\n", state); }

void setFinal(int state) { fprintf(stepFile, "setFinal(%d)\n", state); }

void astAddNode(int index, std::string_view label) {
    std::string s;
    escape_ascii(label, std::back_inserter(s), '\"');
    fprintf(stepFile, "astAddNode(%d, %s)\n", index, s.c_str());
}

void astSetParent(int child, int parent) {
    fprintf(stepFile, "astSetParent(%d, %d)\n", child, parent);
}

void show(std::string_view message) {
    fprintf(stepFile, "show(\"\"\"%s\"\"\")\n", message.data());
}

void section(std::string_view title) {
    fprintf(stepFile, "section(\"\"\"%s\"\"\")\n", title.data());
}

} // namespace step