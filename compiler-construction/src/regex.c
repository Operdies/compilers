#include "regex.h"
#include "collections.h"
#include "logging.h"
#include "macros.h"
#include "text.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// chars 0-10 are not printable and can be freely used as special tokens
#define DIGIT 3 // \d
#define EPSILON 2
#define DOT 1
#define KLEENE '*'
#define PLUS '+'
#define OPTIONAL '?'
#define RANGE '-'

#define add_transition(from, to) (push_dfa(ctx, &(from)->lst, (to)))
#define end_state(state) ((state)->end ? (state)->end : (state))
#define is_dot(state) ((state)->accept == 0)

/* EBNF for regex syntax:
 * regex    = {( class | paren | symbol | union )} [ postfix ] regex | ε
 * class    = "[" {( symbol | range )} "]"
 * paren    = "(" regex ")"
 * postfix  = kleene | plus | optional
 * kleene   = *
 * plus     = +
 * optional = ?
 * union    = regex "|" regex
 * range    = symbol "-" symbol
 * symbol   = char | escaped
 * char     = any char not in [ []().* ]
 * escaped  = "\" [ []().* ]
 */

static dfa *build_automaton(parse_context *ctx);
static bool mk_dfalist(parse_context *ctx, dfalist *lst, size_t cap) {
  dfa **arr = arena_alloc(ctx->a, cap, sizeof(dfa *));
  if (arr) {
    lst->n = 0;
    lst->cap = cap;
    lst->arr = arr;
    return true;
  }
  return false;
}

static void push_dfa(parse_context *ctx, dfalist *lst, dfa *d) {
  if (lst->n >= lst->cap) {
    if (lst->arr == NULL)
      mk_dfalist(ctx, lst, 2);
    else {
      size_t newcap = lst->cap * 2;
      dfa **arr = arena_alloc(ctx->a, newcap, sizeof(dfa *));
      memcpy(arr, lst->arr, lst->n * sizeof(dfa *));
      lst->arr = arr;
      lst->cap = newcap;
    }
  }
  lst->arr[lst->n++] = d;
}

static dfa *mk_state(parse_context *ctx, u8 accept) {
  dfa *d = arena_alloc(ctx->a, sizeof(dfa), 1);
  if (accept == DIGIT) {
    d->accept = '0';
    d->accept_end = '9';
  } else if (accept == DOT) {
    d->accept = 0;
    d->accept_end = ~0;
  } else {
    d->accept = d->accept_end = accept;
  }
  return d;
}

static u8 take_char(parse_context *ctx) {
  if (finished(ctx))
    return 0;
  u8 ch = take(ctx);
  if (ch == '\\') {
    if (finished(ctx)) {
      sprintf(ctx->err, "Escape character at end of expression.");
      return 0;
    }
    ch = take(ctx);
    if (ch == 'n')
      return '\n';
    if (ch == 't')
      return '\t';
    return ch;
  }
  return ch;
}

static dfa *match_symbol(parse_context *ctx, bool class_match) {
  if (finished(ctx))
    return NULL;
  bool escaped = peek(ctx) == '\\';
  u8 ch = take_char(ctx);
  if (ch == 0)
    return NULL;
  if (escaped) {
    if (ch == 'd')
      return mk_state(ctx, DIGIT);
    else
      return mk_state(ctx, ch);
  }

  switch (ch) {
  case '(':
  case ')':
  case '|':
  case '+':
  case '*':
  case '?':
    if (class_match) {
      return mk_state(ctx, ch);
      break;
    }
    // else fall through
  case '[':
  case ']':
    // put it back
    ctx->c--;
    sprintf(ctx->err, "Unescaped literal %c", ch);
    return NULL;
    break;
  case '.':
    return mk_state(ctx, class_match ? ch : DOT);
    break;
  default:
    return mk_state(ctx, ch);
    break;
  }
}

static dfa *match_class(parse_context *ctx) {
  dfa *class, *final;
  bool bitmap[UINT8_MAX] = {0};
  bool negate = false;

  // We can skip the rest of the u8acter class
  // if the first symbol is a dot. We also don't need to mess
  // with epsilon transitions since there is no need for branching
  if (peek(ctx) == '.') {
    while (peek(ctx) != ']' && finished(ctx) == false) {
      advance(ctx);
      if (peek(ctx) == '\\') {
        advance(ctx);
        advance(ctx);
      }
    }
    return mk_state(ctx, DOT);
  }

  if (peek(ctx) == '^') {
    negate = true;
    advance(ctx);
  }

  if (peek(ctx) == ']') {
    sprintf(ctx->err, "Empty character class.");
    return NULL;
  }

  class = mk_state(ctx, EPSILON);
  final = mk_state(ctx, EPSILON);
  class->end = final;

  while (peek(ctx) != ']' && finished(ctx) == false) {
    u8 from = take_char(ctx);
    if (from == 0)
      return NULL;
    u8 to = from;

    if (peek(ctx) == RANGE) {
      advance(ctx);
      to = take_char(ctx);
      if (to == 0)
        return NULL;
      if (to < from) {
        sprintf(ctx->err, "Range contains no values.");
        return NULL;
      }
    }

    for (u8 ch = from; ch <= to; ch++) {
      bitmap[ch] = true;
    }
  }

  if (negate) {
    for (u8 i = 0; i < UINT8_MAX; i++) {
      bitmap[i] = !bitmap[i];
    }
  }

  for (int start = 0; start < UINT8_MAX; start++) {
    if (bitmap[start]) {
      int end;
      for (end = start + 1; end < UINT8_MAX && bitmap[end]; end++) {
      }
      dfa *new = mk_state(ctx, start);
      // accept_end is inclusive
      new->accept_end = end - 1;
      add_transition(new, final);
      add_transition(class, new);
      start = end;
    }
  }

  return class;
}

typedef dfa *(*matcher)(parse_context *);

static dfa *next_match(parse_context *ctx) {
  dfa *result = NULL;
  u8 ch = peek(ctx);
  switch (ch) {
  case '[':
    advance(ctx);
    result = match_class(ctx);
    if (take(ctx) != ']') {
      return NULL;
    }
    return result;
    break;
  case ']':
    sprintf(ctx->err, "Unmatched class terminator.");
    return NULL;
    break;
  case ')':
    sprintf(ctx->err, "Unmatched group terminator.");
    return NULL;
    break;
  case '(':
    advance(ctx);
    result = build_automaton(ctx);
    if (take(ctx) != ')') {
      return NULL;
    }
    break;
  default:
    result = match_symbol(ctx, false);
    break;
  }
  return result;
}

static dfa *build_automaton(parse_context *ctx) {
  dfa *left, *right;
  dfa *next, *start;
  start = next = mk_state(ctx, EPSILON);
  left = right = NULL;

  while (!finished(ctx)) {
    dfa *new = next_match(ctx);
    if (new == NULL) {
      break;
    }

    u8 ch = peek(ctx);
    if (ch == KLEENE || ch == PLUS || ch == OPTIONAL) {
      advance(ctx);

      bool greedy = true;
      bool optional = ch == KLEENE || ch == OPTIONAL;
      bool repeatable = ch == KLEENE || ch == PLUS;

      if (repeatable && peek(ctx) == OPTIONAL) {
        greedy = false;
        advance(ctx);
      }

      dfa *loop_start = mk_state(ctx, EPSILON);
      dfa *loop_end = mk_state(ctx, EPSILON);
      dfa *new_end = end_state(new);

      add_transition(next, loop_start);
      next = loop_end;

      // The order of transitions is crucial because the matching algorithm
      // traverses the automaton in a depth-first fashion. A greedy match is
      // achieved by preferring to enter the loop again, and a non-greedy match
      // is achieved by preferring to exit the loop as early as possible.
      if (greedy) {
        if (repeatable)
          add_transition(new_end, loop_start);

        add_transition(new_end, loop_end);
        add_transition(loop_start, new);
        if (optional)
          add_transition(loop_start, loop_end);

      } else {
        add_transition(new_end, loop_end);
        if (repeatable)
          add_transition(new_end, loop_start);

        if (optional)
          add_transition(loop_start, loop_end);

        add_transition(loop_start, new);
      }
    } else {
      add_transition(next, new);
      next = end_state(new);
    }

    start->end = next;

    if (peek(ctx) == '|') {
      advance(ctx);
      left = start;
      right = build_automaton(ctx);
      if (right == NULL) {
        return NULL;
      }
      dfa *parent = mk_state(ctx, EPSILON);
      add_transition(parent, left);
      add_transition(parent, right);
      start = parent;
      next = mk_state(ctx, EPSILON);
      dfa *left_end = end_state(left);
      dfa *right_end = end_state(right);
      add_transition(left_end, next);
      add_transition(right_end, next);
    }
  }

  start->end = end_state(next);
  return start;
}

static bool match_dfa(dfa *d, match_context *ctx) {
  if (d == NULL)
    return finished(ctx);
  u8 ch;
  if (d->accept != EPSILON) {
    if (finished(ctx))
      return false;
    ch = take(ctx);
    if (ch < d->accept || ch > d->accept_end)
      return false;
  }

  for (size_t i = 0; i < d->lst.n; i++) {
    dfa *next = d->lst.arr[i];
    ssize_t pos = ctx->c;
    // avoid infinite recursion if there is no progress
    if (next->progress == pos)
      continue;
    next->progress = pos;
    if (match_dfa(next, ctx))
      return true;
    ctx->c = pos;
  }

  return finished(ctx) && d->lst.n == 0;
}

static bool partial_match(dfa *d, match_context *ctx) {
  if (d == NULL)
    return finished(ctx);
  u8 ch;
  if (d->accept != EPSILON) {
    if (finished(ctx))
      return false;
    ch = take(ctx);
    if (ch < d->accept || ch > d->accept_end)
      return false;
  }

  for (size_t i = 0; i < d->lst.n; i++) {
    dfa *next = d->lst.arr[i];
    ssize_t pos = ctx->c;
    // avoid infinite recursion if there is no progress
    if (next->progress == pos)
      continue;
    next->progress = pos;
    if (partial_match(next, ctx))
      return true;
    ctx->c = pos;
  }

  return d->lst.n == 0;
}

// Reset the progress of all nodes in the dfa
static void reset(dfa *d) {
  if (d) {
    d->progress = -1;
    for (size_t i = 0; i < d->lst.n; i++) {
      dfa *next = d->lst.arr[i];
      if (next->progress == -1)
        continue;
      reset(next);
    }
  }
}

void destroy_regex(regex *r) {
  if (r)
    destroy_arena(r->ctx.a);
}

regex *mk_regex_from_slice(string_slice slice) {
  regex *r = NULL;
  if (slice.str == NULL)
    die("NULL string");
  if (slice.str) {
    arena *a = mk_arena();
    char *pattern = arena_alloc(a, slice.n + 1, 1);
    strncpy(pattern, slice.str, slice.n);
    r = arena_alloc(a, 1, sizeof(regex));
    r->ctx = (parse_context){.n = slice.n, .c = 0, .src = pattern, .a = a, .err = arena_alloc(a, 100, 1)};
    r->start = build_automaton(&r->ctx);
    reset(r->start);
    if (!finished(&r->ctx)) {
      debug("Invalid regex '%.*s'", slice.n, slice.str);
      destroy_regex(r);
      return NULL;
    }
  }
  return r;
}

regex *mk_regex(const char *pattern) {
  string_slice s = {.n = strlen(pattern), .str = pattern};
  regex *r = mk_regex_from_slice(s);
  return r;
}

regex_match regex_pos(regex *r, const char *string, int len) {
  regex_match d = {0};
  if (!string)
    return d;
  if (len <= 0)
    len = strlen(string);
  match_context m = {.n = len, .c = 0, .src = string};
  reset(r->start);
  bool match = partial_match(r->start, &m);
  regex_match result = match ? (regex_match){.match = true, .start = 0, .length = m.c} : d;
  return result;
}

regex_match regex_matches(regex *r, match_context *ctx) {
  if (r == NULL)
    die("NULL regex");
  size_t pos = ctx->c;
  reset(r->start);
  bool match = partial_match(r->start, ctx);
  if (match) {
    regex_match result = {.match = true, .start = pos, .length = ctx->c - pos};
    return result;
  } else {
    regex_match no_match = {0};
    ctx->c = pos;
    return no_match;
  }
}

bool regex_matches_strict(regex *r, const char *string) {
  match_context m = {.n = strlen(string), .c = 0, .src = string};
  reset(r->start);
  return match_dfa(r->start, &m);
}

regex_match regex_find(regex *r, const char *string) {
  match_context m = {.n = strlen(string), .c = 0, .src = string};
  for (size_t i = 0; i < m.n; i++) {
    reset(r->start);
    m.c = i;
    if (partial_match(r->start, &m)) {
      return (regex_match){
          .start = i,
          .length = m.c - i,
          .match = true,
      };
    }
  }
  return (regex_match){0};
}

bool matches(const char *pattern, const char *string) {
  regex *r = mk_regex(pattern);
  if (r == NULL) {
    return false;
  }
  bool result = regex_matches_strict(r, string);
  destroy_regex(r);
  return result;
}

static void _regex_first(dfa *d, char map[static UINT8_MAX]) {
  if (d == NULL)
    return;
  if (d->accept == EPSILON) {
    for (size_t i = 0; i < d->lst.n; i++) {
      dfa *next = d->lst.arr[i];
      _regex_first(next, map);
    }
  } else {
    for (int ch = d->accept; ch <= d->accept_end; ch++)
      map[ch] = 1;
  }
}

void regex_first(regex *r, char map[static UINT8_MAX]) {
  if (r && r->start) {
    _regex_first(r->start, map);
  }
}
