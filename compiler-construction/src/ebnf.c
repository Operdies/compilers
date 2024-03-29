#include "ebnf.h"
#include "arena.h"
#include "regex.h"
#include <stdio.h>
#include <string.h>

enum terminals {
  LETTER,
  DIGIT,
  STRING,
  ASSIGNMENT,
  PERIOD,
  IDENTIFIER,
  ALTERNATION,
  WHITESPACE,
  LAST_TERMINAL,
};
enum nonterminals {
  SYNTAX = LAST_TERMINAL,
  PRODUCTION,
  EXPRESSION,
  TERM,
  FACTOR,
  LAST_NONTERMINAL,
};

#define LETTERS "a-zA-Z"
#define DIGITS "0-9"
#define SPACES " \n\t"
#define CLASS(x) "[" x "]"
#define KLEENE(x) x "*"

static const char *patterns[] = {
    [ALTERNATION] = KLEENE(CLASS(SPACES)) "\\|" KLEENE(CLASS(SPACES)),
    [WHITESPACE] = KLEENE(CLASS(SPACES)),
    [LETTER] = LETTERS,
    [DIGIT] = DIGITS,
    [STRING] = string_regex,
    [ASSIGNMENT] = KLEENE(CLASS(SPACES)) "=" KLEENE(CLASS(SPACES)),
    [PERIOD] = KLEENE(CLASS(SPACES)) "\\.",
    [IDENTIFIER] = CLASS(LETTERS) KLEENE(CLASS(LETTERS DIGITS)),
};

static regex *regexes[LAST_TERMINAL] = {0};

/* ebnf for ebnf:
 * syntax     = { production }.
 * production = identifier "=" expression ".".
 * expression = term { "|" term }.
 * term       = factor { factor }.
 * factor     = identifier | string | "(" expression ")" | "[" expression "]" | "{" expression "}".
 * identifier = letter { letter | digit}.
 * string     = """ {character} """.
 * letter     = "a" | ... | "z".
 * digit      = "0" | ... | "9".
 *
 * Strategy:
 * A parsing algorithm is derived for each nonterminal symbol, and it is
 * formulated as a procedure carrying the name of the symbol. The occurrence of
 * the symbol in the syntax is translated into a call of the corresponding procedure.
 */

#define MATCH_TERMINAL(terminal)                   \
  {                                                \
    if (!match(g, terminal)) {                     \
      fprintf(stderr, "Expected " #terminal "\n"); \
      return false;                                \
    }                                              \
  }

#define MATCH_NONTERMINAL(nonterminal)                \
  {                                                   \
    if (!nonterminal(g)) {                            \
      fprintf(stderr, "Expected " #nonterminal "\n"); \
      return false;                                   \
    }                                                 \
  }

static bool match(grammar *g, int expr);
static bool syntax(grammar *g);
static bool _production(grammar *g);
static bool expression(grammar *g);
static bool term(grammar *g);
static bool factor(grammar *g);
static bool identifier(grammar *g);

bool match_literal(grammar *g, char literal) {
  if (finished(g->ctx))
    return false;
  if (g->ctx->src[g->ctx->c] == literal) {
    g->ctx->c++;
    return true;
  }
  return false;
}

bool match(grammar *g, int expr) {
  regex *r = regexes[expr];
  regex_match m = regex_matches(r, g->ctx);
  return m.match;
}

bool factor(grammar *g) {
  // first set:
  // letter -> identifier
  // """    -> string
  // "("    -> "(" expression ")"
  // "["    -> "[" expression "]"
  // "{"    -> "{" expression "}"
  MATCH_TERMINAL(WHITESPACE);
  char ch = peek(g->ctx);

  switch (ch) {
  case '"':
  case '\'':
    MATCH_TERMINAL(STRING);
    break;
  case '(':
  case '[':
  case '{':
    advance(g->ctx);
    MATCH_NONTERMINAL(expression);
    char m = ch == '('   ? ')'
             : ch == '[' ? ']'
                         : '}';
    return match_literal(g, m);
    break;
  default:
    return match(g, IDENTIFIER);
  }
  return true;
}

bool term(grammar *g) {
  MATCH_NONTERMINAL(factor);
  while (factor(g)) {
    // loop
  }
  return true;
}

bool expression(grammar *g) {
  MATCH_NONTERMINAL(term);
  while (match(g, ALTERNATION)) {
    MATCH_NONTERMINAL(term);
  }
  return true;
}

bool identifier(grammar *g) {
  MATCH_TERMINAL(IDENTIFIER);
  return true;
}

bool _production(grammar *g) {
  MATCH_TERMINAL(WHITESPACE);
  int id = g->ctx->c;
  MATCH_NONTERMINAL(identifier);
  int id_len = g->ctx->c - id;
  MATCH_TERMINAL(ASSIGNMENT);
  int expr = g->ctx->c;
  MATCH_NONTERMINAL(expression);
  int expr_len = g->ctx->c - expr;
  MATCH_TERMINAL(PERIOD);
  g->ps[g->n] = (production){
      .identifier = (slice){.str = g->ctx->src + id,   .len = id_len  },
      .rule = (slice){.str = g->ctx->src + expr, .len = expr_len},
  };
  return true;
}

void print_slice(slice *s) {
  printf("[%.*s]", s->len, s->str);
}

bool syntax(grammar *g) {
  g->ps = arena_alloc(g->a, 1, sizeof(production));
  while (!finished(g->ctx)) {
    int start = g->ctx->c;
    MATCH_NONTERMINAL(_production);
    int len = g->ctx->c - start;
    slice s = {.str = g->ctx->src + start, .len = len};
    g->ps[g->n].src = s;
    print_slice(&g->ps[g->n].identifier);
    printf(" -> ");
    print_slice(&g->ps[g->n].rule);
    printf("\n");
    g->n++;

    // this arena is only used for productions,
    // so new allocations extend the current list
    arena_alloc(g->a, 1, sizeof(production));
    MATCH_TERMINAL(WHITESPACE);
  }
  return true;
}

grammar *parse_grammar(const char *text) {
  if (!text)
    return NULL;

  for (int i = 0; i < LAST_TERMINAL; i++) {
    if (regexes[i])
      continue;
    regexes[i] = mk_regex(patterns[i]);
    if (!regexes[i]) {
      fprintf(stderr, "Failed to compile regex %s\n", patterns[i]);
      return NULL;
    }
  }

  arena *a = mk_arena();
  grammar *g = arena_alloc(a, 1, sizeof(grammar));
  g->a = a;
  g->ctx = arena_alloc(a, 1, sizeof(match_context));
  *g->ctx = (match_context){.src = text, .n = strlen(text)};
  bool success = syntax(g);
  if (!success)
    return NULL;
  return g;
}
