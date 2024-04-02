#include "ebnf/ebnf.h"
#include "regex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __VA_REST__(A, ...) __VA_ARGS__
#define __VA_FIRST__(A, ...) A

#define panicf(fmt, ...)                                               \
  {                                                                    \
    printf("%s:%d Error: " fmt "\n", __FILE__, __LINE__, __VA_ARGS__); \
    exit(1);                                                           \
  }
#define panic(fmt)                                        \
  {                                                       \
    printf("%s:%d Error: " fmt "\n", __FILE__, __LINE__); \
    exit(1);                                              \
  }

// TODO: error reporting
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

// char pointer to the cursor in the parse context
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
static bool match(parser_t *g, int expr);
static bool syntax(parser_t *g);
static bool production(parser_t *g, production_t *p);
static bool expression(parser_t *g, expression_t *e);
static bool term(parser_t *g, term_t *t);
static bool factor(parser_t *g, factor_t *f);
static bool identifier(parser_t *g, string_slice *s);

static bool match_literal(parser_t *g, char literal) {
  if (finished(&g->ctx))
    return false;
  if (g->ctx.src[g->ctx.c] == literal) {
    g->ctx.c++;
    return true;
  }
  return false;
}

static bool match(parser_t *g, int expr) {
  regex *r = regexes[expr];
  return regex_matches(r, &g->ctx).match;
}

static bool factor(parser_t *g, factor_t *f) {
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
    f->string.str = POINT + 1;
    if (!match(g, STRING)) {
      fprintf(stderr, "Expected STRING\n");
      return 0;
    }
    f->string.n = POINT - f->string.str - 1;
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

bool term(parser_t *g, term_t *t) {
  factor_t f = {0};
  t->range.str = POINT;
  if (!factor(g, &f))
    return false;
  mk_vec(&t->factors_vec, sizeof(factor_t), 1);
  vec_push(&t->factors_vec, &f);
  while (factor(g, &f)) {
    vec_push(&t->factors_vec, &f);
  }
  t->range.n = POINT - t->range.str;
  return true;
}

bool expression(parser_t *g, expression_t *e) {
  mk_vec(&e->terms_vec, sizeof(term_t), 1);

  e->range.str = POINT;
  do {
    term_t t = {0};
    if (!term(g, &t)) {
      destroy_expression(e);
      return false;
    }
    vec_push(&e->terms_vec, &t);
  } while (match(g, ALTERNATION));
  e->range.n = POINT - e->range.str;

  return true;
}

bool identifier(parser_t *g, string_slice *s) {
  s->str = POINT;
  MATCH_TERMINAL(IDENTIFIER);
  s->n = POINT - s->str;
  return true;
}

bool production(parser_t *g, production_t *p) {
  MATCH_TERMINAL(WHITESPACE);
  string_slice s;
  if (!identifier(g, &s))
    return false;
  MATCH_TERMINAL(ASSIGNMENT);
  if (!expression(g, &p->expr))
    return false;
  MATCH_TERMINAL(PERIOD);
  p->identifier = s;
  return true;
}

bool syntax(parser_t *g) {
  mk_vec(&g->productions_vec, sizeof(production_t), 0);
  while (!finished(&g->ctx)) {
    production_t p = {0};
    if (!production(g, &p)) {
      return false;
    }
    vec_push(&g->productions_vec, &p);
    MATCH_TERMINAL(WHITESPACE);
  }
  return true;
}

production_t *find_production(parser_t *g, string_slice name) {
  v_foreach(production_t *, p, g->productions_vec) {
    if (p->identifier.n == name.n)
      if (slicecmp(p->identifier, name) == 0)
        return p;
  }
  return NULL;
}

static bool init_expressions(parser_t *g, expression_t *expr) {
  v_foreach(term_t *, t, expr->terms_vec) {
    v_foreach(factor_t *, f, t->factors_vec) {
      switch (f->type) {

      case F_OPTIONAL:
      case F_REPEAT:
      case F_PARENS: {
        if (!init_expressions(g, &f->expression))
          return false;
        break;
      }
      case F_IDENTIFIER: {
        string_slice name = f->identifier.name;
        production_t *p = find_production(g, name);
        if (p == NULL) {
          fprintf(stderr, "Production '%.*s' not found\n", name.n, name.str);
          return false;
        }
        f->identifier.production = p;
        break;
      }
      case F_STRING:
        break;
      }
    }
  }
  return true;
}

static bool init_productions(parser_t *g) {
  // Iterate through all factors that are identifiers
  // and link back to the production with the matching name
  v_foreach(production_t *, p, g->productions_vec) {
    if (!init_expressions(g, &p->expr))
      return false;
  }
  return true;
}

// #define NEWSYM() (arena_alloc(g->a, 1, sizeof(Symbol)))
#define NEWSYM(empty, nonterminal) (mk_sym(g->a, empty, nonterminal))

Symbol *mk_sym(arena *a, bool empty, bool nonterminal) {
  Symbol *s = arena_alloc(a, 1, sizeof(Symbol));
  *s = (Symbol){.empty = empty, .non_terminal = nonterminal};
  return s;
}

static Symbol *expression_symbol(parser_t *g, expression_t *expr);

Symbol *tail_alt(Symbol *s) {
  Symbol *slow, *fast;
  slow = fast = s;
  for (;;) {
    if (fast->alt == NULL)
      return fast;
    fast = fast->alt;
    if (fast->alt == NULL)
      return fast;
    fast = fast->alt;
    slow = slow->alt;
    if (slow == fast)
      return NULL;
  }
}

Symbol *tail_next(Symbol *s) {
  Symbol *slow, *fast;
  slow = fast = s;
  for (;;) {
    if (fast->next == NULL)
      return fast;
    fast = fast->next;
    if (fast->next == NULL)
      return fast;
    fast = fast->next;
    slow = slow->next;
    if (slow == fast)
      return NULL;
  }
}

bool append_alt(Symbol *chain, Symbol *new_tail) {
  chain = tail_alt(chain);
  if (chain)
    chain->alt = new_tail;
  return chain ? true : false;
}

bool append_next(Symbol *chain, Symbol *new_tail) {
  chain = tail_next(chain);
  if (chain)
    chain->next = new_tail;
  return chain ? true : false;
}

static Symbol *factor_symbol(parser_t *g, factor_t *factor) {
  // mk_vec(&factor_sym.next_vec, sizeof(Symbol), 1);
  switch (factor->type) {
  case F_OPTIONAL:
  case F_REPEAT:
  case F_PARENS: {
    Symbol *subexpression = expression_symbol(g, &factor->expression);
    if (factor->type == F_REPEAT) {
      for (Symbol *alt = subexpression; alt; alt = alt->alt) {
        // TODO: do we also need to loop through the alts of each next here?
        append_next(alt, subexpression);
      }
    }

    if (factor->type == F_OPTIONAL || factor->type == F_REPEAT) {
      Symbol *empty = NEWSYM(true, false);
      if (!append_alt(subexpression, empty)) {
        panic("Circular alt chain prevents loop exit.");
      }
    }
    return subexpression;
  }
  case F_IDENTIFIER: {
    production_t *p = factor->identifier.production;
    if (!p) {
      panicf("Error: unknown terminal %.*s\n", factor->identifier.name.n, factor->identifier.name.str);
      return NULL;
    }
    Symbol *prod = NEWSYM(false, true);
    prod->Nonterminal = p;
    return prod;
  }
  case F_STRING: {
    Symbol *s, *last;
    s = last = NULL;
    string_slice str = factor->string;
    for (int i = 0; i < str.n; i++) {
      char ch = str.str[i];
      Symbol *charsym = NEWSYM(false, false);
      charsym->sym = ch;
      if (s == NULL)
        last = s = charsym;
      else {
        last->next = charsym;
        last = charsym;
      }
    }
    return s;
  }
  default:
    printf("What??\n");
    exit(1);
  }
}

static Symbol *term_symbol(parser_t *g, term_t *term) {
  Symbol *ts, *last;
  ts = last = NULL;
  // mk_vec(&term_sym.next_vec, sizeof(Symbol), 0);
  v_foreach(factor_t *, f, term->factors_vec) {
    Symbol *s = factor_symbol(g, f);
    if (ts == NULL)
      ts = last = s;
    else {
      for (Symbol *alt = last; alt; alt = alt->alt) {
        // TODO: do we also need to loop through the alts of each next here?
        // From cursory testing I could not produce a grammar where this was an issue
        // Tried: Alternators in doubly nested parentheses
        Symbol *next = tail_next(alt);
        if (next && next != s)
          next->next = s;
      }
      last = s;
    }
  }
  return ts;
}

static Symbol *expression_symbol(parser_t *g, expression_t *expr) {
  Symbol *new_expression;
  new_expression = NULL;
  // mk_vec(&expr_sym.alt_vec, sizeof(Symbol), 1);
  v_foreach(term_t *, t, expr->terms_vec) {
    Symbol *new_term = term_symbol(g, t);
    if (!new_expression) {
      new_expression = new_term;
    } else {
      if (!append_alt(new_expression, new_term))
        panic("Circular alt chain.");
    }
  }
  return new_expression;
}

static void build_parse_table(parser_t *g) {
  v_foreach(production_t *, prod, g->productions_vec) {
    prod->header = arena_alloc(g->a, 1, sizeof(Header));
    prod->header->prod = prod;
  }

  v_foreach((void), prod, g->productions_vec) {
    prod->header->sym = expression_symbol(g, &prod->expr);
  }
}

parser_t mk_parser(const char *text) {
  parser_t g = {0};
  if (!text)
    return g;
  g.body = string_from_chars(text, strlen(text));
  g.a = mk_arena();

  for (int i = 0; i < LAST_TERMINAL; i++) {
    if (regexes[i])
      continue;
    regexes[i] = mk_regex(patterns[i]);
    if (!regexes[i]) {
      fprintf(stderr, "Failed to compile regex %s\n", patterns[i]);
      return g;
    }
  }

  g.ctx = (match_context){.src = g.body.chars, .n = g.body.n};
  bool success = syntax(&g);
  if (!success)
    return g;

  if (!init_productions(&g))
    return g;

  build_parse_table(&g);

  return g;
}

static void destroy_expression(expression_t *e);

static void destroy_term(term_t *t) {
  v_foreach(factor_t *, f, t->factors_vec) {
    if (f->type == F_OPTIONAL || f->type == F_REPEAT || f->type == F_PARENS)
      destroy_expression(&f->expression);
  }
  vec_destroy(&t->factors_vec);
}

static void destroy_expression(expression_t *e) {
  v_foreach(term_t *, t, e->terms_vec) { destroy_term(t); }
  vec_destroy(&e->terms_vec);
}

static void destroy_production(production_t *p) {
  destroy_expression(&p->expr);
}

void destroy_parser(parser_t *g) {
  if (g) {
    v_foreach(production_t *, p, g->productions_vec) { destroy_production(p); }
    vec_destroy(&g->productions_vec);
    destroy_string(&g->body);
    destroy_arena(g->a);
  }
}

bool skip(char ch) { return ch == ' ' || ch == '\t' || ch == '\n'; }

bool tokenize(Header *hd, parse_context *ctx, tokens *t) {
  Symbol *x;
  bool match;
  x = hd->sym;
  string_slice name = hd->prod->identifier;
  int start = ctx->c;

  while (x) {
    if (!x->non_terminal) {
      if (!finished(ctx) && x->sym == peek(ctx)) {
        match = true;
        advance(ctx);
      } else {
        match = x->empty;
      }
    } else {
      int here = ctx->c;
      int n_tokens = t->n_tokens;
      match = tokenize(x->Nonterminal->header, ctx, t);
      if (match) {
      }
      if (!match) {
        // rewind
        ctx->c = here;
        t->n_tokens = n_tokens;
      }
    }
    x = match ? x->next : x->alt;
  }
  if (match) {
    string_slice range = {.str = ctx->src + start, .n = ctx->c - start};
    struct token newtoken = {.name = name, .value = range};
    vec_push(&t->tokens_vec, &newtoken);
  }
  return match;
}

tokens parse(parser_t *g, const char *program) {
  tokens t = {0};
  mk_vec(&t.tokens_vec, sizeof(struct token), 0);
  printf("Parse program: %s\n", program);
  parse_context ctx = {.n = strlen(program), .src = program};
  Header *start = g->productions[0].header;
  if (!tokenize(start, &ctx, &t)) {
    panic("Failed to parse program\n");
  }
  if (!finished(&ctx)) {
    panicf("Parse ended prematurely:\n"
           "%.*s\n"
           " %*s\n",
           (int)(ctx.n), ctx.src, (int)ctx.c, "^");
  }
  return t;
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

static void populate_terminals(terminal_list *terminals, expression_t *e) {
  v_foreach(term_t *, t, e->terms_vec) {
    v_foreach(factor_t *, f, t->factors_vec) {
      if (f->type == F_PARENS || f->type == F_OPTIONAL || f->type == F_REPEAT) {
        populate_terminals(terminals, &f->expression);
      } else if (f->type == F_STRING) {
        vec_push(&terminals->terminals_vec, (char *)f->string.str);
      }
    }
  }
}

terminal_list get_terminals(const parser_t *g) {
  terminal_list t = {0};
  mk_vec(&t.terminals_vec, sizeof(char), 0);
  v_foreach(production_t *, p, g->productions_vec) {
    populate_terminals(&t, &p->expr);
  }
  return t;
}
nonterminal_list get_nonterminals(const parser_t *g) {
  nonterminal_list t = {0};
  mk_vec(&t.nonterminals_vec, sizeof(Header), 0);
  v_foreach(production_t *, p, g->productions_vec)
      vec_push(&t.nonterminals_vec, p->header);
  return t;
}

static void populate_first_expr(const parser_t *g, struct Header *h, expression_t *e);

static void populate_first_term(const parser_t *g, struct Header *h, term_t *t) {
  v_foreach(factor_t *, fac, t->factors_vec) {
    switch (fac->type) {
    case F_OPTIONAL:
    case F_REPEAT:
      // these can be skipped, so the next term must be included in the first set
      populate_first_expr(g, h, &fac->expression);
      continue;
    case F_PARENS:
      populate_first_expr(g, h, &fac->expression);
      return;
    case F_IDENTIFIER: {
      // Add the first set of this production to this first set
      struct Header *id = fac->identifier.production->header;
      populate_first(g, id);
      vec_push_slice(&h->first_vec, &id->first_vec.slice);
      return;
    }
    case F_STRING:
      vec_push(&h->first_vec, (char *)fac->string.str);
      return;
    }
  }
}

static void populate_first_expr(const parser_t *g, struct Header *h, expression_t *e) {
  v_foreach(term_t *, t, e->terms_vec) {
    populate_first_term(g, h, t);
  }
}

void populate_first(const parser_t *g, struct Header *h) {
  if (h->first) {
    return;
  }
  mk_vec(&h->first_vec, sizeof(char), 0);
  populate_first_expr(g, h, &h->prod->expr);
}

/* follow set
 *
 */

void populate_follow_expr(const parser_t *g, Header *h, expression_t *e);

void populate_follow_term(const parser_t *g, Header *h, term_t *t) {
}

void populate_follow_expr(const parser_t *g, Header *h, expression_t *e) {
  v_foreach(term_t *, t, e->terms_vec) {
    populate_follow_term(g, h, t);
  }
}

void populate_follow(const parser_t *g, struct Header *h) {
  if (h->follow) {
    return;
  }
  mk_vec(&h->follow_vec, sizeof(char), 0);
  populate_follow_expr(g, h, &h->prod->expr);
}

bool is_ll1(const parser_t *g);
