#include "ebnf.h"
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

#define POINT (g->ctx.src + g->ctx.c)

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

static void destroy_expression(expression_t *e);
static bool match(grammar_t *g, int expr);
static bool syntax(grammar_t *g);
static bool production(grammar_t *g, production_t *p);
static bool expression(grammar_t *g, expression_t *e);
static bool term(grammar_t *g, term_t *t);
static bool factor(grammar_t *g, factor_t *f);
static bool identifier(grammar_t *g, string_slice *s);

// static char *factor_to_string(enum factor_switch f) {
//   switch (f) {
//   case F_ERROR:
//     return "error";
//   case F_OPTIONAL:
//     return "optional";
//   case F_REPEAT:
//     return "repeat";
//   case F_PARENS:
//     return "parens";
//   case F_IDENTIFIER:
//     return "identifier";
//   case F_STRING:
//     return "string";
//   default:
//     return "unknown";
//   }
// }

static bool match_literal(grammar_t *g, char literal) {
  if (finished(&g->ctx))
    return false;
  if (g->ctx.src[g->ctx.c] == literal) {
    g->ctx.c++;
    return true;
  }
  return false;
}

static bool match(grammar_t *g, int expr) {
  regex *r = regexes[expr];
  return regex_matches(r, &g->ctx).match;
}

static bool factor(grammar_t *g, factor_t *f) {
  // first set:
  // letter -> identifier
  // """    -> string
  // "("    -> "(" expression ")"
  // "["    -> "[" expression "]"
  // "{"    -> "{" expression "}"
  MATCH_TERMINAL(WHITESPACE);
  f->range.str = POINT;
  char ch = peek(&g->ctx);

  switch (ch) {
  case '"':
  case '\'':
    f->type = F_STRING;
    MATCH_TERMINAL(STRING);
    break;
  case '(':
    f->type = F_PARENS;
    advance(&g->ctx);
    if (!expression(g, &f->expression))
      return false;
    if (!match_literal(g, ')')) {
      fprintf(stderr, "Unmatched ')' in factor\n");
      return false;
    }
    break;
  case '[':
    f->type = F_OPTIONAL;
    advance(&g->ctx);
    if (!expression(g, &f->expression))
      return false;
    if (!match_literal(g, ']')) {
      fprintf(stderr, "Unmatched ']' in factor\n");
      return false;
    }
    break;
  case '{':
    f->type = F_REPEAT;
    advance(&g->ctx);
    if (!expression(g, &f->expression))
      return false;
    if (!match_literal(g, '}')) {
      fprintf(stderr, "Unmatched '}' in factor\n");
      return false;
    }
    break;
  default: {
    string_slice s = {.str = POINT};
    if (!match(g, IDENTIFIER)) {
      return false;
    }
    s.n = POINT - s.str;
    f->type = F_IDENTIFIER;
    f->identifier.name = s;
    break;
  }
  }
  f->range.n = POINT - f->range.str;
  return true;
}

bool term(grammar_t *g, term_t *t) {
  factor_t f = {0};
  t->range.str = POINT;
  if (!factor(g, &f))
    return false;
  mk_vec(&t->v, sizeof(factor_t), 1);
  vec_push(&t->v, &f);
  while (factor(g, &f)) {
    vec_push(&t->v, &f);
  }
  t->range.n = POINT - t->range.str;
  return true;
}

bool expression(grammar_t *g, expression_t *e) {
  mk_vec(&e->v, sizeof(term_t), 1);

  e->range.str = POINT;
  do {
    term_t t = {0};
    if (!term(g, &t)) {
      destroy_expression(e);
      return false;
    }
    vec_push(&e->v, &t);
  } while (match(g, ALTERNATION));
  e->range.n = POINT - e->range.str;

  return true;
}

bool identifier(grammar_t *g, string_slice *s) {
  s->str = POINT;
  MATCH_TERMINAL(IDENTIFIER);
  s->n = POINT - s->str;
  return true;
}

bool production(grammar_t *g, production_t *p) {
  MATCH_TERMINAL(WHITESPACE);
  string_slice s;
  if (!identifier(g, &s))
    return false;
  MATCH_TERMINAL(ASSIGNMENT);
  if (!expression(g, &p->expr))
    return false;
  MATCH_TERMINAL(PERIOD);
  p->identifier = s;
  p->rule = p->expr.range;
  return true;
}

static bool syntax(grammar_t *g) {
  mk_vec(&g->v, sizeof(production_t), 0);
  while (!finished(&g->ctx)) {
    int start = g->ctx.c;
    production_t p = {0};
    if (!production(g, &p)) {
      return false;
    }
    int len = g->ctx.c - start;
    string_slice s = {.str = g->ctx.src + start, .n = len};
    p.src = s;
    vec_push(&g->v, &p);
    MATCH_TERMINAL(WHITESPACE);
  }
  return true;
}

grammar_t parse_grammar(const char *text) {
  grammar_t error = {0};
  if (!text)
    return error;

  for (int i = 0; i < LAST_TERMINAL; i++) {
    if (regexes[i])
      continue;
    regexes[i] = mk_regex(patterns[i]);
    if (!regexes[i]) {
      fprintf(stderr, "Failed to compile regex %s\n", patterns[i]);
      return error;
    }
  }

  grammar_t g = {0};
  g.ctx = (match_context){.src = text, .n = strlen(text)};
  bool success = syntax(&g);
  if (!success)
    return error;

  printf("Alphabet:\n");
  for (int i = 0; i < g.n; i++) {
    production_t *p = g.productions + i;
    char name[50];
    snprintf(name, sizeof(name), "%.*s", p->identifier.n, p->identifier.str);
    for (int j = 0; j < p->expr.n; j++) {
      term_t *t = p->expr.terms + j;
      for (int k = 0; k < t->n; k++) {
        factor_t *f = t->factors + k;
        if (f->type == F_STRING) {
          printf("%.*s", f->range.n - 2, f->range.str + 1);
          // const char *type = factor_to_string(f->type);
          // printf(" [%s] %.*s", type, f->range.n, f->range.str);
        }
      }
    }
  }
  puts("");
  return g;
}

static void destroy_term(term_t *t) {
  vec_destroy(&t->v);
}

static void destroy_expression(expression_t *e) {
  for (int i = 0; i < e->n; i++) {
    destroy_term(e->terms + i);
  }
  vec_destroy(&e->v);
}

static void destroy_production(production_t *p) {
  destroy_expression(&p->expr);
}

void destroy_grammar(grammar_t *g) {
  if (g) {
    for (int i = 0; i < g->n; i++) {
      destroy_production(g->productions + i);
    }
    vec_destroy(&g->v);
  }
}

position_t get_position(const char *source, string_slice place) {
  int line, column;
  line = column = 1;
  for (; source && *source; source++) {
    if (source == place.str)
      return (position_t){.line = line, .column = column};
    if (*source == '\n') {
      line++;
      column = 1;
    } else
      column++;
  }
  return (position_t){-1, -1};
}
parser_t tokenize(const grammar_t *g, const char *body) {
  parser_t error = {0};
  return error;
}
