#ifndef TEXT_H
#define TEXT_H
#include "arena.h"

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

#endif // TEXT_H
