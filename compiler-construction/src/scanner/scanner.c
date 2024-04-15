#include "scanner/scanner.h"
#include "logging.h"
#include <string.h>

static bool _tokenize(scanner *s, parse_context *ctx, vec *tokens) {
  while (!finished(ctx)) {
    bool found = false;
    string_slice value = {.str = ctx->src + ctx->c};
    v_foreach(token *, t, s->tokens) {
      regex_match m = regex_matches(t->pattern, ctx);
      if (m.match) {
        found = true;
        value.n = (ctx->src + ctx->c) - value.str;
        token_t tok = {.id = t->id,
                       .value = value};
        vec_push(tokens, &tok);
        break;
      }
    }
    if (!found)
      return false;
  }
  return true;
}
void tokenize(scanner *s, const char *body, vec *tokens) {
  parse_context ctx = {.src = body, .n = strlen(body)};
  if (!_tokenize(s, &ctx, tokens))
    die("No match");
}

void add_token(scanner *s, const char *expression, const char *name) {
  regex *r = mk_regex(expression);
  int id = s->tokens.n;
  vec_push(&s->tokens, &(token){.pattern = r, .name = strdup(name), .id = id});
}

void mk_scanner(scanner *s, int n, token_def tokens[static n]) {
  *s = (scanner){0};
  s->tokens.sz = sizeof(token);
  for (int i = 0; i < n; i++) {
    token_def *t = &tokens[i];
    regex *r = mk_regex(t->pattern);
    if (r == NULL)
      die("Failed to parse regex from %s", t->pattern);
    token n = {.pattern = r, .name = t->name, .id = i};
    vec_push(&s->tokens, &n);
  }
}
