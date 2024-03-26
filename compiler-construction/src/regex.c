#include "regex.h"
#include "arena.h"
#include "macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// chars 0-10 are not printable and can be freely used as special tokens
#define EPSILON 2
#define DOT 1
#define KLEENE '*'
#define OPTIONAL '?'

#define finished(ctx) ((ctx)->c >= (ctx)->n)
#define peek(ctx) (finished((ctx)) ? -1 : (ctx)->src[(ctx)->c])
#define take(ctx) (finished((ctx)) ? -1 : (ctx)->src[(ctx)->c++])
#define advance(ctx) ((ctx)->c++);
#define add_transition(from, to) (push_dfa(ctx, &(from)->lst, (to)))
#define end_state(state) ((state)->end ? (state)->end : (state))

/* EBNF for regex syntax:
 * regex    = {( group | paren | symbol | union )} postfix regex | ε
 * group    = "[" { literal } "]"
 * paren    = "(" regex ")"
 * postfix  = kleene | optional | ε
 * kleene   = *
 * optional = ?
 * union    = regex "|" regex
 * symbol   = char | escaped
 * char     = any char not in [ []().* ]
 * escaped  = "\" [ []().* ]
 */

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
  char accept;
  // Index into the source string
  size_t index;
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
typedef parse_context match_context;

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
  d->index = ctx->c;
  d->accept = accept;
  return d;
}

dfa *match_symbol(parse_context *ctx) {
  if (finished(ctx))
    return NULL;
  char ch = take(ctx);
  if (ch == '\\') {
    if (finished(ctx))
      return NULL;
    return mk_state(ctx, take(ctx));
  }
  switch (ch) {
  case '[':
  case ']':
  case '(':
  case ')':
  case '|':
  case '?':
    sprintf(ctx->err, "Illegal literal %c", ch);
    return NULL;
    break;
  case '.':
    return mk_state(ctx, DOT);
    break;
  default:
    return mk_state(ctx, ch);
    break;
  }
}

dfa *match_class(parse_context *ctx) {
  dfa *class, *final;
  class = mk_state(ctx, EPSILON);
  final = mk_state(ctx, EPSILON);
  class->end = final;

  while (peek(ctx) != ']') {
    dfa *new = match_symbol(ctx);
    add_transition(new, final);
    add_transition(class, new);
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

    if (peek(ctx) == KLEENE) {
      advance(ctx);
      dfa *loop = mk_state(ctx, EPSILON);
      dfa *loop_exit = mk_state(ctx, EPSILON);
      dfa *loop_end = end_state(new);
      new->end = loop;
      add_transition(loop_end, loop);
      add_transition(next, loop);
      add_transition(loop, new);
      add_transition(loop, loop_exit);
      next = loop_exit;
    } else if (peek(ctx) == OPTIONAL) {
      advance(ctx);
      dfa *new_end = mk_state(ctx, EPSILON);
      dfa *end = end_state(new);
      add_transition(next, new);
      add_transition(next, new_end);
      add_transition(end, new_end);
      next = new_end;
      new->end = next;
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
    if (d->accept != ch && d->accept != DOT)
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

// Reset the progress of all nodes in the dfa
void reset(dfa *d){
  if (d){
    d->progress = -1;
    for (size_t i = 0; i < d->lst.n; i++) {
      dfa *next = d->lst.arr[i];
      if (next->progress == -1) continue;
      reset(next);
    }
  }
}

bool matches(char *pattern, char *string) {
  parse_context ctx = {
      .n = strlen(pattern), .c = 0, .src = pattern, .a = mk_arena()};
  dfa *d = build_automaton(&ctx);
  if (!finished((&ctx))) {
    d = NULL;
  }
  if (d == NULL) {
    fprintf(stderr, "Failed to construct automata: %s\n", ctx.err);
    return false;
  }
  match_context m = {.n = strlen(string), .c = 0, .src = string};
  reset(d);
  bool result = match_dfa(d, &m);
  destroy_arena(ctx.a);
  return result;
}
