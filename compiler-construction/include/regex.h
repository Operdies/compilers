#ifndef REGEX_H
#define REGEX_H
#include "arena.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct dfa dfa;

typedef struct {
  size_t n;
  size_t cap;
  dfa **arr;
} dfalist;

struct dfa {
  // Possible transitions from this state
  dfalist lst;
  // Character accepted by this state
  unsigned char accept;
  // If set and greater than accept, all characters in the range [accept-accept_end] (inclusive) are accepted
  unsigned char accept_end;
  // The end state of this dfa
  // If NULL, the state itself is considered the end state
  dfa *end;
  // The progress the last time this state was visited.
  // This is used to detect loops when traversing the automaton
  ssize_t progress;
};

typedef struct {
  // cursor
  size_t c;
  // length
  size_t n;
  // text being parsed
  const char *src;
  // A description of the parse error when a matcher returns NULL
  char err[50];
  // A scratch area to allocate parse constructs
  arena *a;
} parse_context;

typedef struct {
  bool match;
  size_t start;
  size_t length;
} regex_match;

typedef parse_context match_context;

typedef struct {
  parse_context ctx;
  dfa *start;
} regex;

bool matches(const char *pattern, const char *string);
regex_match regex_pos(regex *r, const char *string, int len);
bool regex_matches(regex *r, const char *string);
regex_match regex_find(regex *r, const char *string);
void destroy_regex(regex *r);
regex *mk_regex(const char *pattern);
#endif // REGEX_H
