#pragma once

#include <string.h>

#include "collections.h"
#define finished(ctx) ((ctx)->c >= (ctx)->view.n)
#define peek(ctx) (finished((ctx)) ? -1 : (ctx)->view.str[(ctx)->c])
#define take(ctx) (finished((ctx)) ? -1 : (ctx)->view.str[(ctx)->c++])
#define advance(ctx) ((ctx)->c++);

typedef struct {
  string_slice view;  // The text being parsed
  int c;              // cursor
} parse_context;

#define SINGLETICK_STR "'([^'\\\\]|\\\\.)*'"
#define DOUBLETICK_STR "\"([^\"\\\\]|\\\\.)*\""
static const char string_regex[] = SINGLETICK_STR "|" DOUBLETICK_STR;
