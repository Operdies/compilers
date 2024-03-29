#ifndef REGEX_H
#define REGEX_H
#include "arena.h"
#include "text.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct dfa dfa;
typedef unsigned char u8;

typedef struct {
  size_t n;
  size_t cap;
  dfa **arr;
} dfalist;

struct dfa {
  // Possible transitions from this state
  dfalist lst;
  // Character accepted by this state
  u8 accept;
  // If set and greater than accept, all characters in the range [accept-accept_end] (inclusive) are accepted
  u8 accept_end;
  // The end state of this dfa
  // If NULL, the state itself is considered the end state
  dfa *end;
  // The progress the last time this state was visited.
  // This is used to detect loops when traversing the automaton
  ssize_t progress;
};

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
bool regex_matches_strict(regex *r, const char *string);
regex_match regex_matches(regex *r, match_context *ctx);
regex_match regex_find(regex *r, const char *string);
void destroy_regex(regex *r);
regex *mk_regex(const char *pattern);
#endif // REGEX_H
