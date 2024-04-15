#pragma once

#include "regex.h"
typedef struct {
  regex *pattern;
  char *name;
  int id;
} token;

typedef struct {
  char* pattern;
  char *name;
} token_def;

typedef struct {
  int id;
  string_slice value;
} token_t;

typedef struct {
  vec tokens;
} scanner;

void tokenize(scanner *s, const char *body, vec *tokens);
void add_token(scanner *s, const char *expression, const char *name);
void mk_scanner(scanner *s, int n, token_def tokens[static n]);
