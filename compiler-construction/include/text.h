#ifndef TEXT_H
#define TEXT_H
#include "arena.h"

#define finished(ctx) ((ctx)->c >= (ctx)->n)
#define peek(ctx) (finished((ctx)) ? -1 : (ctx)->src[(ctx)->c])
#define take(ctx) (finished((ctx)) ? -1 : (ctx)->src[(ctx)->c++])
#define advance(ctx) ((ctx)->c++);

typedef struct {
  // cursor
  size_t c;
  // length
  size_t n;
  // text being parsed
  const char *src;
  // A description of the parse error when a matcher returns NULL
  char *err;
  // A scratch area to allocate parse constructs
  arena *a;
} parse_context;

// TODO: move this to a sensible location
#define SINGLETICK_STR  "'([^'\\\\]|\\\\.)*'"
#define DOUBLETICK_STR "\"([^\"\\\\]|\\\\.)*\""
static const char string_regex[] = SINGLETICK_STR "|" DOUBLETICK_STR;

#endif // TEXT_H
