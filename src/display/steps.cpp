#include <cstdio>
#include <stdarg.h>
#include "src/display/steps.h"

extern FILE *stepFile;

const char *bool_str[2] = {"False", "True"};

const int N_STATES = 2048;

void stepPrepare(int nsym, int nprod) {
  fprintf(stepFile, 1 + R"(
#!nsym=%d,nprod=%d

from collections import deque
)", nsym, nprod);
}

void stepFinish() {
//     fprintf(stepFile, R"(
// print(to_json(symbol))
// print(to_json(production))
// )");
}

void stepSymbol(int id, const char *name, bool is_term, bool is_start) {
    fprintf(stepFile, "symbol[%d].name='%s'\n", id, name);
    fprintf(stepFile, "symbol[%d].is_term=%s\n", id, bool_str[is_term]);
    fprintf(stepFile, "symbol[%d].is_start=%s\n", id, bool_str[is_start]);
    // fprintf(stepFile, "symbol[%d]['productions']=[]\n", id);
    // fprintf(stepFile, "symbol[%d] = { ", id);
    // fprintf(stepFile, "'name' : '%s', ", name);
    // fprintf(stepFile, "'is_term' : %s, ", bool_str[is_term]);
    // fprintf(stepFile, "'is_start' : %s, ", bool_str[is_start]);
    // fprintf(stepFile, "'productions' : [], ");
    // fprintf(stepFile, "'first' : set(), ");
    // fprintf(stepFile, "'follow' : set(), ");
    // fprintf(stepFile, "'nullable' : None, ");
    // fprintf(stepFile, "}\n");
    // fprintf(stepFile, "symbol[%d] = Symbol('%s', %s, %s)\n", id, name,
    //         bool_str[is_term], bool_str[is_start]);
}

void stepProduction(int id, int head, const int *body, size_t body_size) {
    fprintf(stepFile, "production[%d].head = %d\n", id, head);
    fprintf(stepFile, "production[%d].body = [", id);
    for (size_t index = 0; index < body_size; ++index) {
      fprintf(stepFile, index ? ", %d" : "%d", body[index]);
    }
    fprintf(stepFile, "]\n");
    // fprintf(stepFile, "production[%d] = Production(%d, [", id, head);
    // for (size_t index = 0; index < body_size; ++index) {
    //   fprintf(stepFile, index ? ", %d" : "%d", body[index]);
    // }
    // fprintf(stepFile, "])\n");
    fprintf(stepFile, "symbol[%d].productions.append(%d)\n", head, id);
    
}

void stepNullable(int symbol, bool nullable, const char *explain) {
    fprintf(stepFile, "symbol[%d].nullable = %-5s \n# %s\n", symbol,
            bool_str[nullable], explain);
}

void stepFirstAdd(int symbol, int component, const char *explain) {
    fprintf(stepFile, "symbol[%d].first.add(%d) \n# %s\n", symbol,
            component, explain);
}

void stepFollowAdd(int symbol, int component, const char *explain) {
    fprintf(stepFile, "symbol[%d].follow.add(%d) \n# %s\n", symbol, component,
            explain);
}

void stepFollowMerge(int dest, int src, const char *explain) {
    fprintf(stepFile,
            "symbol[%d].follow.update(symbol[%d].follow) \n# %s\n", dest,
            src, explain);
}

void stepTableAdd(int state, int look_ahead, const char *action) {
    fprintf(stepFile, "table[%d][%d].add('%s')\n", state, look_ahead, action);
}

void stepTestInit() {
//     fprintf(stepFile, R"(
// symbol_stack = deque()
// state_stack = deque()
// input_queue = deque()
// )");
}

void stepPrintf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stepFile, fmt, ap);  
}
