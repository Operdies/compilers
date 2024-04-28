#include "scanner/scanner.h"

#include <stdlib.h>
#include <string.h>

#include "collections.h"
#include "logging.h"

int peek_token(scanner *s, const bool *valid, string_slice *content) {
  int here = s->ctx->c;
  int token = next_token(s, valid, content);
  s->ctx->c = here;
  return token;
}

bool match_slice(scanner *s, string_slice slice, string_slice *content) {
  string_slice compare = {.str = s->ctx->src + s->ctx->c, .n = slice.n};
  if (s->ctx->n < s->ctx->c + compare.n) return false;

  if (slicecmp(slice, compare) == 0) {
    s->ctx->c += compare.n;
    if (content) *content = compare;
    return true;
  }

  return false;
}

bool match_token(scanner *s, int kind, string_slice *content) {
  while (peek(s->ctx) == ' ' || peek(s->ctx) == '\n' || peek(s->ctx) == '\t') advance(s->ctx);
  if (finished(s->ctx)) return false;

  token *t = (token *)s->tokens.array + kind;

  regex_match m = regex_matches(t->pattern, s->ctx);
  if (m.match && content) *content = m.matched;

  while (peek(s->ctx) == ' ' || peek(s->ctx) == '\n' || peek(s->ctx) == '\t') advance(s->ctx);
  return m.match;
}

int next_token(scanner *s, const bool *valid, string_slice *content) {
  int tok = ERROR_TOKEN;
  while (peek(s->ctx) == ' ' || peek(s->ctx) == '\n' || peek(s->ctx) == '\t') advance(s->ctx);

  if (finished(s->ctx)) return EOF_TOKEN;

  v_foreach(token *, t, s->tokens) {
    if (valid == NULL || valid[idx_t]) {
      regex_match m = regex_matches(t->pattern, s->ctx);
      if (m.match) {
        if (content) *content = m.matched;
        tok = idx_t;
        break;
      }
    }
  }
  while (peek(s->ctx) == ' ' || peek(s->ctx) == '\n' || peek(s->ctx) == '\t') advance(s->ctx);
  return tok;
}

void rewind_scanner(scanner *s, string_slice point) { s->ctx->c = point.str - s->ctx->src; }

static bool _tokenize(scanner *s, parse_context *ctx, vec *tokens) {
  while (!finished(ctx)) {
    bool found = false;
    string_slice value = {.str = ctx->src + ctx->c};
    v_foreach(token *, t, s->tokens) {
      regex_match m = regex_matches(t->pattern, ctx);
      if (m.match) {
        found = true;
        value.n = (ctx->src + ctx->c) - value.str;
        token_t tok = {.id = t->id, .value = value};
        vec_push(tokens, &tok);
        break;
      }
    }
    if (!found) return false;
  }
  return true;
}

void tokenize(scanner *s, const char *body, vec *tokens) {
  parse_context ctx = {.src = body, .n = strlen(body)};
  if (!_tokenize(s, &ctx, tokens)) die("No match");
}

scanner mk_scanner(const scanner_tokens tokens) {
  scanner s = {0};
  s.tokens.sz = sizeof(token);
  for (int i = 0; i < tokens.n; i++) {
    const token_def *t = &tokens.tokens[i];
    token n = {0};
    if (t->pattern) {
      regex *r = mk_regex(t->pattern);
      if (r == NULL) die("Failed to parse regex from %s", t->pattern);
      n = (token){.pattern = r, .name = mk_slice(t->name), .id = i};
    }
    vec_push(&s.tokens, &n);
  }
  return s;
}
void destroy_scanner(scanner *s) {
  v_foreach(token *, t, s->tokens) { destroy_regex(t->pattern); }
  vec_destroy(&s->tokens);
}
