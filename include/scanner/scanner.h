#ifndef SCANNER_H
#define SCANNER_H

#define ERROR_TOKEN (-1)
#define EOF_TOKEN (-2)

#include "regex.h"
typedef struct {
  regex *pattern;
  string_slice name;
  int id;
} token;

typedef struct {
  char *name;
  char *pattern;
} token_def;

typedef struct {
  int id;
  string_slice value;
} token_t;

typedef struct {
  vec tokens;
  parse_context *ctx;
} scanner;

typedef struct {
  int n;
  const token_def *tokens;
} scanner_tokens;
#define mk_tokens(t) \
  (scanner_tokens) { .n = LENGTH(t), .tokens = t }

int peek_token(scanner *s, const bool *valid, string_slice *content);
int next_token(scanner *s, const bool *valid, string_slice *content);
bool match_slice(scanner *s, string_slice slice, string_slice *content);
bool match_token(scanner *s, int kind, string_slice *content);
void tokenize(scanner *s, const char *body, vec *tokens);
void add_token(scanner *s, const char *expression, const char *name);
scanner mk_scanner(const scanner_tokens tokens);
void rewind_scanner(scanner *s, string_slice point);
void destroy_scanner(scanner *s);

#endif  // SCANNER_H
