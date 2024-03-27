#include "regex.h"
#include "macros.h"
#include <stdio.h>
#include <string.h>

// chars 0-10 are not printable and can be freely used as special tokens
#define EPSILON 2
#define DOT 1
#define KLEENE '*'
#define PLUS '+'
#define OPTIONAL '?'

#define finished(ctx) ((ctx)->c >= (ctx)->n)
#define peek(ctx) (finished((ctx)) ? -1 : (ctx)->src[(ctx)->c])
#define take(ctx) (finished((ctx)) ? -1 : (ctx)->src[(ctx)->c++])
#define advance(ctx) ((ctx)->c++);
#define add_transition(from, to) (push_dfa(ctx, &(from)->lst, (to)))
#define end_state(state) ((state)->end ? (state)->end : (state))
#define is_dot(state) ((state)->accept == 0)

/* EBNF for regex syntax:
 * regex    = {( group | paren | symbol | union )} postfix regex | ε
 * group    = "[" {( symbol | range )} "]"
 * paren    = "(" regex ")"
 * postfix  = kleene | plus | optional | ε
 * kleene   = *
 * plus     = +
 * optional = ?
 * union    = regex "|" regex
 * range    = symbol "-" symbol
 * symbol   = char | escaped
 * char     = any char not in [ []().* ]
 * escaped  = "\" [ []().* ]
 */

dfa *build_automaton(parse_context *ctx);
bool mk_dfalist(parse_context *ctx, dfalist *lst, size_t cap) {
  dfa **arr = arena_alloc(ctx->a, cap, sizeof(dfa *));
  if (arr) {
    lst->n = 0;
    lst->cap = cap;
    lst->arr = arr;
    return true;
  }
  return false;
}

void push_dfa(parse_context *ctx, dfalist *lst, dfa *d) {
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

dfa *mk_state(parse_context *ctx, char accept) {
  dfa *d = arena_alloc(ctx->a, sizeof(dfa), 1);
  if (accept == DOT) {
    d->accept = 0;
    d->accept_end = ~0;
  } else {
    d->accept = d->accept_end = accept;
  }
  return d;
}

dfa *match_symbol(parse_context *ctx) {
  if (finished(ctx))
    return NULL;
  char ch = peek(ctx);
  if (ch == '\\') {
    advance(ctx);
    if (finished(ctx))
      return NULL;
    return mk_state(ctx, take(ctx));
  }
  dfa *s = NULL;
  switch (ch) {
  case '[':
  case ']':
  case '(':
  case ')':
  case '|':
  case '+':
  case '*':
  case '?':
    sprintf(ctx->err, "Unescaped literal %c", ch);
    return NULL;
    break;
  case '.':
    s = mk_state(ctx, DOT);
    advance(ctx);
    break;
  default:
    s = mk_state(ctx, ch);
    advance(ctx);
    break;
  }
  return s;
}

dfa *match_class(parse_context *ctx) {
  dfa *class, *final;
  class = mk_state(ctx, EPSILON);
  final = mk_state(ctx, EPSILON);
  class->end = final;

  while (peek(ctx) != ']') {
    dfa *new = match_symbol(ctx);
    if (new == NULL)
      return NULL;

    add_transition(new, final);
    add_transition(class, new);

    if (peek(ctx) == '-') {
      advance(ctx);
      dfa *end = match_symbol(ctx);
      if (end == NULL)
        return NULL;
      if (is_dot(new)) {
        sprintf(ctx->err, "Range cannot start with DOT.");
        return NULL;
      }
      if (is_dot(end)) {
        sprintf(ctx->err, "Range cannot end with DOT.");
        return NULL;
      }
      if (end->accept <= new->accept) {
        sprintf(ctx->err, "Range contains no values.");
        return NULL;
      }
      new->accept_end = end->accept;
    }
  }
  return class;
}

typedef dfa *(*matcher)(parse_context *);

dfa *next_match(parse_context *ctx) {
  dfa *result = NULL;
  char ch = peek(ctx);
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
    result = match_symbol(ctx);
    break;
  }
  return result;
}

dfa *build_automaton(parse_context *ctx) {
  dfa *left, *right;
  dfa *next, *start;
  start = next = mk_state(ctx, EPSILON);
  left = right = NULL;

  while (!finished(ctx)) {
    dfa *new = next_match(ctx);
    if (new == NULL) {
      break;
    }

    char ch = peek(ctx);
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

/*
 * Does the current state match the current character
 */
bool accept(dfa *d, match_context *ctx) {
  // If finished, there are no charactesr left, reject
  if (finished(ctx))
    return false;
  // If DOT, match anything
  if (d->accept == DOT)
    return true;
  // Otherwise, match if equal
  return d->accept == peek(ctx);
}

bool match_dfa(dfa *d, match_context *ctx) {
  if (d == NULL)
    return finished(ctx);
  char ch;
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

bool partial_match(dfa *d, match_context *ctx) {
  if (d == NULL)
    return finished(ctx);
  char ch;
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
void reset(dfa *d) {
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

regex *mk_regex(const char *pattern) {
  regex *r = NULL;
  if (pattern) {
    arena *a = mk_arena();
    r = arena_alloc(a, 1, sizeof(regex));
    size_t len = strlen(pattern);
    char *src = arena_alloc(a, len + 1, 1);
    strcpy(src, pattern);
    r->ctx = (parse_context){.n = len, .c = 0, .src = src, .a = a};
    r->start = build_automaton(&r->ctx);
    reset(r->start);
    if (!finished(&r->ctx)) {
      destroy_regex(r);
      return NULL;
    }
  }
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

bool regex_matches(regex *r, const char *string) {
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
  bool result = regex_matches(r, string);
  destroy_regex(r);
  return result;
}
