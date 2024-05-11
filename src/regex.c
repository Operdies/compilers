#include "regex.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "collections.h"
#include "logging.h"
#include "macros.h"
#include "text.h"

// chars 0-10 are not printable and can be freely used as special tokens
#define DIGIT 3  // \d
#define EPSILON 2
#define DOT 1
#define KLEENE '*'
#define PLUS '+'
#define OPTIONAL '?'
#define RANGE '-'

#define add_transition(from, to) (push_nfa(&(from)->lst, (to)))
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

static arena *regex_arena = NULL;

static nfa *build_automaton(parse_context *ctx, char terminator);
static bool mk_nfalist(arena *a, nfalist *lst, size_t cap) {
  nfa **arr = arena_alloc(a, cap, sizeof(nfa *));
  if (arr) {
    lst->n = 0;
    lst->cap = cap;
    lst->arr = arr;
    return true;
  }
  return false;
}

static void push_nfa(nfalist *lst, nfa *d) {
  if (lst->n >= lst->cap) {
    if (lst->arr == NULL)
      mk_nfalist(regex_arena, lst, 2);
    else {
      size_t newcap = lst->cap * 2;
      nfa **arr = arena_alloc(regex_arena, newcap, sizeof(nfa *));
      memcpy(arr, lst->arr, lst->n * sizeof(nfa *));
      lst->arr = arr;
      lst->cap = newcap;
    }
  }
  lst->arr[lst->n++] = d;
}

static nfa *mk_state(u8 accept) {
  nfa *d = arena_alloc(regex_arena, sizeof(nfa), 1);
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
      error("Escape character at end of expression.");
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

static nfa *match_symbol(parse_context *ctx, bool class_match) {
  if (finished(ctx))
    return NULL;
  bool escaped = peek(ctx) == '\\';
  u8 ch = take_char(ctx);
  if (ch == 0)
    return NULL;
  if (escaped) {
    if (ch == 'd')
      return mk_state(DIGIT);
    else
      return mk_state(ch);
  }

  switch (ch) {
    case '(':
    case ')':
    case '|':
    case '+':
    case '*':
    case '?':
      if (class_match) {
        return mk_state(ch);
        break;
      }
      // else fall through
    case '[':
    case ']':
      // put it back
      ctx->c--;
      error("Unescaped literal %c", ch);
      return NULL;
      break;
    case '.':
      return mk_state(class_match ? ch : DOT);
      break;
    default:
      return mk_state(ch);
      break;
  }
}

static nfa *match_class(parse_context *ctx) {
  nfa *class, *final;
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
    return mk_state(DOT);
  }

  if (peek(ctx) == '^') {
    negate = true;
    advance(ctx);
  }

  if (peek(ctx) == ']') {
    error("Empty character class.");
    return NULL;
  }

  class = mk_state(EPSILON);
  final = mk_state(EPSILON);
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
        error("Range contains no values.");
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
      nfa *new = mk_state(start);
      // accept_end is inclusive
      new->accept_end = end - 1;
      add_transition(new, final);
      add_transition(class, new);
      start = end;
    }
  }

  return class;
}

typedef nfa *(*matcher)(parse_context *);

static nfa *next_match(parse_context *ctx, char terminator) {
  nfa *result = NULL;
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
    case ']': {
      if (']' != terminator)
        error("Unmatched class terminator.");
      return NULL;
    } break;
    case ')':
      if (')' != terminator)
        error("Unmatched group terminator.");
      return NULL;
      break;
    case '(':
      advance(ctx);
      result = build_automaton(ctx, ')');
      if (take(ctx) != ')')
        return NULL;
      break;
    default:
      result = match_symbol(ctx, false);
      break;
  }
  return result;
}

static nfa *build_automaton(parse_context *ctx, char terminator) {
  nfa *left, *right;
  nfa *next, *start;
  start = next = mk_state(EPSILON);
  left = right = NULL;

  while (!finished(ctx)) {
    nfa *new = next_match(ctx, terminator);
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

      nfa *loop_start = mk_state(EPSILON);
      nfa *loop_end = mk_state(EPSILON);
      nfa *new_end = end_state(new);

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
      right = build_automaton(ctx, terminator);
      if (right == NULL) {
        return NULL;
      }
      nfa *parent = mk_state(EPSILON);
      add_transition(parent, left);
      add_transition(parent, right);
      start = parent;
      next = mk_state(EPSILON);
      nfa *left_end = end_state(left);
      nfa *right_end = end_state(right);
      add_transition(left_end, next);
      add_transition(right_end, next);
    }
  }

  start->end = end_state(next);
  return start;
}

static bool match_nfa(nfa *d, match_context *ctx) {
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
    nfa *next = d->lst.arr[i];
    ssize_t pos = ctx->c;
    // avoid infinite recursion if there is no progress
    if (next->progress == pos)
      continue;
    next->progress = pos;
    if (match_nfa(next, ctx))
      return true;
    ctx->c = pos;
  }

  return finished(ctx) && d->lst.n == 0;
}

static bool partial_match(nfa *d, match_context *ctx) {
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
    nfa *next = d->lst.arr[i];
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

// Reset the progress of all nodes in the nfa
static void reset(nfa *d) {
  if (d) {
    d->progress = -1;
    for (size_t i = 0; i < d->lst.n; i++) {
      nfa *next = d->lst.arr[i];
      if (next->progress == -1)
        continue;
      reset(next);
    }
  }
}

void destroy_regex(regex *r) { (void)r; }

regex *mk_regex_from_slice(string_slice slice) {
  if (!regex_arena) {
    regex_arena = mk_arena();
    atexit_r((cleanup_func)destroy_arena, regex_arena);
  }

  regex *r = NULL;
  if (slice.str == NULL)
    error("NULL string");
  if (slice.str) {
    char *copy = arena_alloc(regex_arena, slice.n + 1, 1);
    memcpy(copy, slice.str, slice.n);
    r = arena_alloc(regex_arena, 1, sizeof(regex));
    r->ctx = (parse_context){
        .view = {.n = slice.n, .str = copy}
    };
    r->start = build_automaton(&r->ctx, 0);
    reset(r->start);
    if (!finished(&r->ctx)) {
      debug("Invalid regex '%S'", slice);
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
  regex_match result = {0};
  if (!string)
    return result;
  if (len <= 0)
    len = strlen(string);
  match_context m = {
      .view = {.n = len, .str = string}
  };
  reset(r->start);
  bool match = partial_match(r->start, &m);
  if (match) {
    result.match = true;
    result.matched = (string_slice){.n = m.c, .str = string};
  }
  return result;
}

regex_match regex_matches(regex *r, match_context *ctx) {
  if (r == NULL)
    error("NULL regex");
  size_t pos = ctx->c;
  reset(r->start);
  bool match = partial_match(r->start, ctx);
  if (match) {
    regex_match result = {0};
    result.match = true;
    result.matched = (string_slice){.n = ctx->c - pos, .str = ctx->view.str + pos};
    return result;
  } else {
    regex_match no_match = {0};
    ctx->c = pos;
    return no_match;
  }
}

bool regex_matches_strict(regex *r, const char *string) {
  match_context m = mk_ctx(string);
  reset(r->start);
  return match_nfa(r->start, &m);
}

regex_match regex_find(regex *r, const char *string) {
  match_context m = mk_ctx(string);
  for (int i = 0; i < m.view.n; i++) {
    reset(r->start);
    m.c = i;
    if (partial_match(r->start, &m)) {
      return (regex_match){
          .matched = {.n = m.c - i, .str = m.view.str + i},
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

static void _regex_first(nfa *d, char map[static UINT8_MAX]) {
  if (d == NULL)
    return;
  if (d->accept == EPSILON) {
    for (size_t i = 0; i < d->lst.n; i++) {
      nfa *next = d->lst.arr[i];
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
