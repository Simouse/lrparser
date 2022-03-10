#ifndef STEPS_H
#define STEPS_H

void stepPrepare(int nsym, int nprod);
void stepFinish();
void stepNullable(int symbol, bool nullable, const char *explain);
void stepSymbol(int id, const char *name, bool is_term, bool is_start);
void stepProduction(int id, int head, const int *body, size_t body_size);
void stepFirstAdd(int symbol, int component, const char *explain);
void stepFollowAdd(int symbol, int component, const char *explain);
void stepFollowMerge(int dest, int src, const char *explain);
void stepTableAdd(int state, int look_ahead, const char *action);
void stepTestInit();
void stepPrintf(const char *fmt, ...);

#endif
