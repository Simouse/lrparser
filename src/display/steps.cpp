#include <cstdio>
#include <stdarg.h>
#include "src/display/steps.h"

extern FILE *stepFile;

const char *bool_str[2] = {"False", "True"};

const int N_STATES = 2048;

void stepPrepare(int nsym, int nprod) {
  fprintf(stepFile, 1 + R"(
from collections import deque
)");
//     fprintf(stepFile, 1 + R"(
// import json
// from collections import deque

// class Symbol:
//   def __init__(self, name: str, is_term: bool, is_start: bool):
//     self.name = name
//     self.is_term = is_term
//     self.is_start = is_start
//     self.productions = []
//     self.nullable = None
//     self.first = set()
//     self.follow = set()

// class Production:
//   def __init__(self, head: int, body: list):
//     self.head = head
//     self.body = body

// class Encoder(json.JSONEncoder):
//    def default(self, obj):
//       if isinstance(obj, set):
//         return list(obj)
//       if isinstance(obj, (Symbol, Production)):
//         return obj.__dict__
//       return json.JSONEncoder.default(self, obj)

// def to_json(obj):
//   return json.dumps(obj, cls=Encoder, sort_keys=False, indent=4)

// symbol = [None for _ in range(0, %d)]
// production = [{} for _ in range(0, %d)]
// table = [[set() for _ in range(0, %d)] for _ in range(0, %d)]

// )", nsym, nprod, nsym, N_STATES);
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
