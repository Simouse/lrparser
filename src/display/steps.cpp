#include <stdarg.h>
#include <string>
#include <algorithm>
#include <iostream>
#include "src/display/steps.h"

extern FILE *stepFile;

const char *bool_str[2] = {"False", "True"};

const int N_STATES = 2048;

template<class OutIter>
OutIter escape_unicode(std::string const& s, OutIter out, char quote) {
  if (quote)
    *out++ = quote;

  for (std::string::const_iterator i = s.begin(), end = s.end(); i != end; ++i) {
    unsigned char c = *i;
    if (' ' <= c && c <= '~' && c != '\\' && c != '"') {
      *out++ = c;
    } 
    else {
      *out++ = '\\';
      switch(c) {
      case '"':  *out++ = '"';  break;
      case '\\': *out++ = '\\'; break;
      case '\t': *out++ = 't';  break;
      case '\r': *out++ = 'r';  break;
      case '\n': *out++ = 'n';  break;
      case '\'': *out++ = '\''; break;
      default:
        char const* const hexdig = "0123456789ABCDEF";
        *out++ = 'x';
        *out++ = hexdig[c >> 4];
        *out++ = hexdig[c & 0xF];
      }
    }
  }

  if (quote)
    *out++ = quote;

  return out;
}

template<class OutIter>
OutIter escape_ascii(std::string const& s, OutIter out, char quote) {
  if (quote)
    *out++ = quote;
  for (std::string::const_iterator i = s.begin(), end = s.end(); i != end; ++i) {
    unsigned char c = *i;
    if (' ' <= c && c <= '~' && c != '\\' && c != '"') {
      *out++ = c;
    } 
    else {
      switch(c) {
      case '"':  *out++ = '\\'; *out++ = '"';  break;
      case '\\': *out++ = '\\'; *out++ = '\\'; break;
      case '\t': *out++ = '\\'; *out++ = 't';  break;
      case '\r': *out++ = '\\'; *out++ = 'r';  break;
      case '\n': *out++ = '\\'; *out++ = 'n';  break;
      case '\'': *out++ = '\\'; *out++ = '\''; break;
      default:   *out++ = c;
      }
    }
  }
  if (quote)
    *out++ = quote;
  return out;
}

void stepPrepare(int nsym, int nprod) {
//   fprintf(stepFile, 1 + R"(
// #!nsym=%d,nprod=%d

// )", nsym, nprod);
}

void stepFinish() {
//     fprintf(stepFile, R"(
// print(to_json(symbol))
// print(to_json(production))
// )");
}

void stepSymbol(int id, const char *name, bool is_term, bool is_start) {
    std::string escaped_name;
    escape_ascii(name, std::back_inserter(escaped_name), '\'');
    fprintf(stepFile, "symbol[%d].name=%s\n", id, escaped_name.c_str());
    fprintf(stepFile, "symbol[%d].is_term=%s\n", id, bool_str[is_term]);
    fprintf(stepFile, "symbol[%d].is_start=%s\n", id, bool_str[is_start]);
}

void stepProduction(int id, int head, const int *body, size_t body_size) {
    fprintf(stepFile, "production[%d].head = %d\n", id, head);
    fprintf(stepFile, "production[%d].body = [", id);
    for (size_t index = 0; index < body_size; ++index) {
      fprintf(stepFile, index ? ", %d" : "%d", body[index]);
    }
    fprintf(stepFile, "]\n");
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
