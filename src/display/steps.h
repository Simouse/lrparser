#ifndef STEPS_H
#define STEPS_H

#include <string_view>

namespace step {

// void stepPrepare(int nsym, int nprod);
// void stepFinish();
void nullable(int symbol, bool nullable, const char *explain);
void symbol(int id, const char *name, bool is_term, bool is_start);
void production(int id, int head, const int *body, size_t body_size);
void addFirst(int symbol, int component, const char *explain);
void addFollow(int symbol, int component, const char *explain);
void mergeFirst(int dest, int src, const char *explain);
void mergeFollow(int dest, int src, const char *explain);
void mergeFollowFromFirst(int dest, int src, int eps, const char *explain);
void addTableEntry(int state, int look_ahead, const char *action);
void printf(const char *fmt, ...);
void addState(int state, std::string_view description);
void updateState(int state, std::string_view description);
void addEdge(int s1, int s2, std::string_view label);
void setStart(int state);
void setFinal(int state);
void astAddNode(int index, std::string_view label);
void astSetParent(int child, int parent);
// void astSetRoot(int root);
void show(std::string_view message);
// This function can receive null messages so program won't crash.
// When a null message is received, the main label is unchanged.
void show(const char *message);
void section(std::string_view title);

}

#endif
