#ifndef SCANNER_H
#define SCANNER_H

#define ERROR_TOKEN (-1)
#define EOF_TOKEN (-2)

#include "regex.h"
typedef struct {
  regex *pattern;
  char *name;
  int id;
} token;

typedef struct {
  char *pattern;
  char *name;
} token_def;

typedef struct {
  int id;
  string_slice value;
} token_t;

typedef struct {
  vec tokens;
  parse_context *ctx;
} scanner;

int next_token(scanner *s, const int *valid, string_slice *content);
void tokenize(scanner *s, const char *body, vec *tokens);
void add_token(scanner *s, const char *expression, const char *name);
void mk_scanner(scanner *s, int n, token_def tokens[static n]);
void rewind_scanner(scanner *s, string_slice point);

#endif // SCANNER_H