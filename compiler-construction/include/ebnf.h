#include "collections.h"
#include "text.h"
#include "arena.h"

typedef struct {
  const char *str;
  int len;
} slice;

typedef struct {
  slice identifier;
  slice rule;
  slice src;
} production;

typedef struct {
  size_t n;
  production *ps;
  parse_context *ctx;
  arena *a;
} grammar;

grammar *parse_grammar(const char *grammar);
