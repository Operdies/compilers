#include "ebnf/ebnf.h"
#include "logging.h"
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

#define MATCH_TERMINAL(terminal)       \
  {                                    \
    if (!match(g, terminal)) {         \
      die("Expected " #terminal "\n"); \
      return false;                    \
    }                                  \
  }

void destroy_expression(expression_t *e);
bool match(parser_t *g, int expr);
bool syntax(parser_t *g);
bool production(parser_t *g, production_t *p);
bool expression(parser_t *g, expression_t *e);
bool term(parser_t *g, term_t *t);
bool factor(parser_t *g, factor_t *f);
bool identifier(parser_t *g, string_slice *s);

static bool match_literal(parser_t *g, char literal) {
  if (finished(&g->ctx))
    return false;
  if (g->ctx.src[g->ctx.c] == literal) {
    g->ctx.c++;
    return true;
  }
  return false;
}

bool match(parser_t *g, int expr) {
  regex *r = regexes[expr];
  return regex_matches(r, &g->ctx).match;
}

bool factor(parser_t *g, factor_t *f) {
  MATCH_TERMINAL(WHITESPACE);
  f->range.str = POINT;
  char ch = peek(&g->ctx);

  switch (ch) {
  case '"':
  case '\'':
    f->type = F_STRING;
    string_slice string;
    string.str = POINT + 1;

    if (!match(g, STRING)) {
      fprintf(stderr, "Expected STRING\n");
      return 0;
    }
    string.n = POINT - string.str - 1;
    f->regex = mk_regex_from_slice(string);
    if (f->regex == NULL)
      die("Failed to construct regex from '%.*s'", string.n, string.str);
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
  t->factors_vec = v_make(factor_t);
  vec_push(&t->factors_vec, &f);
  while (factor(g, &f)) {
    vec_push(&t->factors_vec, &f);
  }
  t->range.n = POINT - t->range.str;
  return true;
}

bool expression(parser_t *g, expression_t *e) {
  e->terms_vec = v_make(term_t);

  e->range.str = POINT;
  do {
    term_t t = {0};
    if (!term(g, &t)) {
      die("Expected term at %s.", e->range.str);
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
  {
    if (!match(g, IDENTIFIER)) {
      die("Expected IDENTIFIER in identifier %s\n", s->str);
      return 0;
    }
  };
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
  {
    if (!match(g, PERIOD)) {
      error("Expected PERIOD in production %.*s\n", s.n, s.str);
      return 0;
    }
  };
  p->identifier = s;
  return true;
}

bool syntax(parser_t *g) {
  mk_vec(&g->productions_vec, sizeof(production_t), 1);
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

#define MKSYM() (symbol_t *)arena_alloc(g->a, 1, sizeof(symbol_t))

static symbol_t *expression_symbol(parser_t *g, expression_t *expr);

symbol_t *tail_alt(symbol_t *s) {
  symbol_t *slow, *fast;
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

symbol_t *tail_next(symbol_t *s) {
  symbol_t *slow, *fast;
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

bool append_alt(symbol_t *chain, symbol_t *new_tail) {
  chain = tail_alt(chain);
  if (chain)
    chain->alt = new_tail;
  return chain ? true : false;
}

bool append_next(symbol_t *chain, symbol_t *new_tail) {
  chain = tail_next(chain);
  if (chain)
    chain->next = new_tail;
  return chain ? true : false;
}

void append_all_nexts(symbol_t *head, symbol_t *tail, vec *seen) {
  if (vec_contains(seen, head))
    return;
  vec_push(seen, head);
  for (; head && head != tail; head = head->alt) {
    if (head->next == NULL)
      head->next = tail;
    else
      append_all_nexts(head->next, tail, seen);
  }
}

static symbol_t *factor_symbol(parser_t *g, factor_t *factor) {
  switch (factor->type) {
  case F_OPTIONAL:
  case F_REPEAT:
  case F_PARENS: {
    symbol_t *subexpression = expression_symbol(g, &factor->expression);
    if (factor->type == F_REPEAT) {
      symbol_t *loop = MKSYM();
      *loop = (symbol_t){.empty = true};
      {
        // ensure that all nexts of the subexpression can repeat the loop
        vec seen = v_make(symbol_t);
        append_all_nexts(subexpression, loop, &seen);
        vec_destroy(&seen);
      }
      loop->next = subexpression;
      subexpression = loop;
      symbol_t *empty = MKSYM();
      *empty = (symbol_t){.empty = true};
      loop->alt = empty;
    } else if (factor->type == F_OPTIONAL) {
      symbol_t *empty = MKSYM();
      *empty = (symbol_t){.empty = true};
      {
        // ensure that all nexts of the subexpression lead to the thing that follows this optional
        vec seen = v_make(symbol_t);
        append_all_nexts(subexpression, empty, &seen);
        vec_destroy(&seen);
      }
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
    symbol_t *prod = MKSYM();
    *prod = (symbol_t){.is_nonterminal = true, .nonterminal = p};
    return prod;
  }
  case F_STRING: {
    symbol_t *s = MKSYM();
    *s = (symbol_t){.regex = factor->regex};
    // symbol_t *s, *last;
    // s = last = NULL;
    // string_slice str = factor->string;
    // for (int i = 0; i < str.n; i++) {
    //   char ch = str.str[i];
    //   symbol_t *charsym = MKSYM();
    //   *charsym = (symbol_t){.sym = ch};
    //   if (s == NULL)
    //     last = s = charsym;
    //   else {
    //     last->next = charsym;
    //     last = charsym;
    //   }
    // }
    return s;
  }
  default:
    printf("What??\n");
    exit(1);
  }
}

static symbol_t *term_symbol(parser_t *g, term_t *term) {
  symbol_t *ts, *last;
  ts = last = NULL;
  v_foreach(factor_t *, f, term->factors_vec) {
    symbol_t *s = factor_symbol(g, f);
    if (ts == NULL)
      ts = last = s;
    else {
      for (symbol_t *alt = last; alt; alt = alt->alt) {
        // TODO: do we also need to loop through the alts of each next here?
        // From cursory testing I could not produce a grammar where this was an issue
        // Tried: Alternators in doubly nested parentheses
        symbol_t *next = tail_next(alt);
        if (next && next != s)
          next->next = s;
      }
      last = s;
    }
  }
  return ts;
}

static symbol_t *expression_symbol(parser_t *g, expression_t *expr) {
  symbol_t *new_expression;
  new_expression = NULL;
  v_foreach(term_t *, t, expr->terms_vec) {
    symbol_t *new_term = term_symbol(g, t);
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
    prod->header = arena_alloc(g->a, 1, sizeof(header_t));
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
    string_slice s = {.n = strlen(patterns[i]), .str = patterns[i]};
    regexes[i] = mk_regex_from_slice(s);
    if (!regexes[i]) {
      die("Failed to compile regex %s\n", patterns[i]);
    }
  }

  g.ctx = (match_context){.src = g.body.chars, .n = g.body.n};
  bool success = syntax(&g);
  if (!success)
    die("Failed to parse grammar.");

  if (!init_productions(&g))
    return g;

  build_parse_table(&g);

  return g;
}

void destroy_expression(expression_t *e);

static void destroy_term(term_t *t) {
  v_foreach(factor_t *, f, t->factors_vec) {
    if (f->type == F_OPTIONAL || f->type == F_REPEAT || f->type == F_PARENS)
      destroy_expression(&f->expression);
  }
  vec_destroy(&t->factors_vec);
}

void destroy_expression(expression_t *e) {
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

bool tokenize(header_t *hd, parse_context *ctx, tokens *t, bool backtrack) {
  symbol_t *x;
  bool match;
  struct parse_frame {
    size_t source_cursor;
    int token_cursor;
    symbol_t *symbol;
  };

  vec alt_stack = {.sz = sizeof(struct parse_frame)};

  x = hd->sym;
  string_slice name = hd->prod->identifier;
  int start = ctx->c;

  while (x) {
    struct parse_frame frame = {.source_cursor = ctx->c, .token_cursor = t->n_tokens};

    if (x->is_nonterminal) {
      match = tokenize(x->nonterminal->header, ctx, t, backtrack);
      if (backtrack && !match) {
        // rewind
        ctx->c = frame.source_cursor;
        t->n_tokens = frame.token_cursor;
      }
    } else {
      if (x->empty) {
        match = true;
      } else {
        regex_match m = regex_matches(x->regex, ctx);
        match = m.match;
      }
    }

    { // pick next state, and potentially store the alt option for later
      symbol_t *next = x->next;
      symbol_t *alt = x->alt;
      // If the 'next' option is used, push a frame so the alt option can be tried instead.
      if (alt && match) {
        frame.symbol = alt;
        vec_push(&alt_stack, &frame);
      }
      x = match ? next : alt;
    }

    // If an end symbol was reached without a match, check if a suitable frame can be restored
    if (x == NULL && match == false) {
      struct parse_frame *f = NULL;
      if ((f = vec_pop(&alt_stack))) {
        if (backtrack) {
          // If backtracking is enabled, rewind the cursors to the stored frame
          x = f->symbol;
          ctx->c = f->source_cursor;
          t->n_tokens = f->token_cursor;
        } else if (f->source_cursor == ctx->c && f->token_cursor == t->n_tokens) {
          // Otherwise restore the symbol from the frame unless progress was made
          x = f->symbol;
        }
      }
    }
  }

  int len = ctx->c - start;
  if (match && len > 0) {
    string_slice range = {.str = ctx->src + start, .n = len};
    struct token_t newtoken = {.name = name, .value = range};
    vec_push(&t->tokens_vec, &newtoken);
  }
  vec_destroy(&alt_stack);
  return match;
}

bool parse(parser_t *g, const char *program, tokens *result) {
  if (result == NULL)
    return false;
  if (result->tokens) {
    vec_clear(&result->tokens_vec);
  } else {
    mk_vec(&result->tokens_vec, sizeof(struct token_t), 0);
  }
  parse_context ctx = {.n = strlen(program), .src = program};
  header_t *start = g->productions[0].header;
  result->success = tokenize(start, &ctx, result, g->backtrack) && finished(&ctx);
  result->error.ctx = ctx;

  if (!result->success || !finished(&ctx)) {
    snprintf(result->error.error, sizeof(result->error.error),
             "Parse error at:\n"
             "%.*s\n"
             " %*s\n",
             (int)(ctx.n), ctx.src, (int)ctx.c, "^");
    debug("Parse error at:\n"
          "%.*s\n"
          " %*s\n",
          (int)(ctx.n), ctx.src, (int)ctx.c, "^");
    return false;
  }
  return true;
}
