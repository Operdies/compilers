#include "ebnf/ebnf.h"
#include "collections.h"
#include "logging.h"
#include "macros.h"
#include "regex.h"
#include "scanner/scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static bool match(parser_t *g, enum terminals expr);
bool syntax(parser_t *g);
bool production(parser_t *g, production_t *p);
bool expression(parser_t *g, expression_t *e);
bool term(parser_t *g, term_t *t);
bool factor(parser_t *g, factor_t *f);
bool identifier(parser_t *g, string_slice *s);

void print_ast(AST *root, vec *parents) {
#define arr ((char *)vbuf.array)
  static vec vbuf = {.sz = sizeof(char)};
  const char fork[] = "├";
  const char angle[] = "└";
  const char dash[] = "──";
  const char pipe[] = "│";

  if (!root)
    return;

  vbuf.n = 0;
  vec _marker = v_make(AST);

  if (!parents)
    parents = &_marker;

  for (; root; root = root->next) {
    vbuf.n = 0;
    if (root->range.str) {
      v_foreach(AST *, p, (*parents))
          vec_write(&vbuf, "%s   ", p->next ? pipe : " ");
      vec_write(&vbuf, "%s", root->next ? fork : angle);
      vec_write(&vbuf, "%s ", dash);

      int lim = root->range.n;
      char *newline = strchr(root->range.str, '\n');
      if (newline) {
        lim = (newline - root->range.str);
        if (lim > root->range.n)
          lim = root->range.n;
      }

      vec_write(&vbuf, "%.*s", root->name.n, root->name.str);
      int wstrlen = 0;
      int max = 70;

      // count utf-8 code points
      for (int i = 0; i < vbuf.n; i++)
        wstrlen += (arr[i] & 0xC0) != 0x80;
      vec_write(&vbuf, "%*s '%.*s'%s", max - wstrlen, "<->   ", lim, root->range.str, lim < root->range.n ? "..." : "");

      for (int i = 0; i < vbuf.n; i++)
        if (arr[i] == '\n')
          arr[i] = '^';
        else if (arr[i] == '\t')
          arr[i] = '>';

      vec_push(&vbuf, &(char){0});
      debug(vbuf.array);
    } else {
      debug("Nothing?");
    }
    vec_push(parents, root);
    print_ast(root->first_child, parents);
    vec_pop(parents);
  }
  vec_destroy(&_marker);
#undef arr
}

void destroy_ast(AST *root) {
  while (root) {
    destroy_ast(root->first_child);
    AST *tmp = root;
    root = root->next;
    free(tmp);
  }
}

#define mk_ast() ecalloc(1, sizeof(AST))

static bool match_literal(parser_t *g, char literal) {
  if (finished(&g->ctx))
    return false;
  if (g->ctx.src[g->ctx.c] == literal) {
    g->ctx.c++;
    return true;
  }
  return false;
}

bool match(parser_t *g, enum terminals expr) {
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
    string_slice string = {0};
    string.str = POINT + 1;

    if (!match(g, STRING)) {
      fprintf(stderr, "Expected STRING\n");
      return 0;
    }
    string.n = POINT - string.str - 1;
    f->string = string;
    if (string.n == 0)
      die("String of length 0 in grammar.");
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

token *find_token(parser_t *g, string_slice name) {
  v_foreach(token *, t, g->s->tokens) {
    if (t->name.n == name.n)
      if (slicecmp(t->name, name) == 0)
        return t;
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
        if (p) {
          f->identifier.production = p;
          break;
        }
        token *t = find_token(g, name);
        if (t) {
          f->type = F_TOKEN;
          f->token = t;
          break;
        }
        error("Production '%.*s' not found\n", name.n, name.str);
        return false;
        break;
      }
      case F_STRING:
        break;
      case F_TOKEN:
        die("How can this happen??");
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
    p->id = idx_p;
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

static symbol_t *make_repeatable(parser_t *g, symbol_t *subexpression) {
  symbol_t *loop = MKSYM();
  *loop = (symbol_t){.type = empty_symbol};

  // ensure that all nexts of the subexpression can repeat the loop
  vec seen = v_make(symbol_t);
  append_all_nexts(subexpression, loop, &seen);
  vec_destroy(&seen);

  loop->next = subexpression;
  subexpression = loop;
  symbol_t *empty = MKSYM();
  *empty = (symbol_t){.type = empty_symbol};
  loop->alt = empty;
  return subexpression;
}

static symbol_t *make_optional(parser_t *g, symbol_t *subexpression) {
  symbol_t *empty = MKSYM();
  *empty = (symbol_t){.type = empty_symbol};

  // ensure that all nexts of the subexpression lead to the thing that follows this optional
  vec seen = v_make(symbol_t);
  append_all_nexts(subexpression, empty, &seen);
  vec_destroy(&seen);

  if (!append_alt(subexpression, empty)) {
    die("Circular alt chain prevents loop exit.");
  }
  return subexpression;
}

struct factor_symbols {
  symbol_t *head;
  symbol_t *tail;
};

static struct factor_symbols factor_symbol(parser_t *g, factor_t *factor) {
  switch (factor->type) {
  case F_OPTIONAL:
  case F_REPEAT:
  case F_PARENS: {
    symbol_t *subexpression = expression_symbol(g, &factor->expression);
    if (factor->type == F_REPEAT) {
      subexpression = make_repeatable(g, subexpression);
    } else if (factor->type == F_OPTIONAL) {
      subexpression = make_optional(g, subexpression);
    }

    // Expressions can have many terminating states.
    // Here we consolidate the terminating states in a single empty symbol.
    symbol_t *tail = MKSYM();
    *tail = (symbol_t){.type = empty_symbol};
    vec seen = v_make(symbol_t);
    append_all_nexts(subexpression, tail, &seen);
    vec_destroy(&seen);

    return (struct factor_symbols){.head = subexpression, .tail = tail};
  }
  case F_IDENTIFIER: {
    production_t *p = factor->identifier.production;
    if (!p) {
      die("Error: unknown terminal %.*s\n", factor->identifier.name.n, factor->identifier.name.str);
    }
    symbol_t *prod = MKSYM();
    *prod = (symbol_t){.type = nonterminal_symbol, .nonterminal = p};
    return (struct factor_symbols){prod, prod};
  }
  case F_STRING: {
    symbol_t *s = MKSYM();
    *s = (symbol_t){.type = string_symbol, .string = factor->string};
    return (struct factor_symbols){s, s};
  }
  case F_TOKEN: {
    symbol_t *s = MKSYM();
    *s = (symbol_t){.type = token_symbol, .token = factor->token};
    return (struct factor_symbols){s, s};
  }
  default:
    die("What??\n");
  }
}

static symbol_t *term_symbol(parser_t *g, term_t *term) {
  symbol_t *head, *tail;
  head = tail = NULL;
  v_foreach(factor_t *, f, term->factors_vec) {
    struct factor_symbols factors = factor_symbol(g, f);
    if (head == NULL) {
      head = factors.head;
    } else {
      tail->next = factors.head;
    }
    tail = factors.tail;
  }
  return head;
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
        die("Circular alt chain.");
    }
  }
  return new_expression;
}

static bool build_parse_table(parser_t *g) {
  v_foreach(production_t *, prod, g->productions_vec) {
    prod->header = arena_alloc(g->a, 1, sizeof(header_t));
    prod->header->prod = prod;
  }

  v_foreach((void), prod, g->productions_vec) {
    prod->header->sym = expression_symbol(g, &prod->expr);
  }
  return true;
}

static void init_parser(parser_t *g, scanner s) {
  *g = (parser_t){0};
  g->a = mk_arena();
  g->s = arena_alloc(g->a, 1, sizeof(scanner));
  *g->s = s;
  if (!regexes[0]) {
    for (int i = 0; i < LAST_TERMINAL; i++) {
      string_slice s = {.n = strlen(patterns[i]), .str = patterns[i]};
      regexes[i] = mk_regex_from_slice(s);
      if (!regexes[i]) {
        die("Failed to compile regex %s\n", patterns[i]);
      }
    }
  }
}

static bool finalize_parser(parser_t *g) {
  if (!init_productions(g))
    return false;

  return build_parse_table(g);
}

parser_t mk_parser(grammar_rules rules, scanner_tokens tokens) {
  scanner s = mk_scanner(tokens);
  parser_t g = {0};
  init_parser(&g, s);
  g.productions_vec = v_make(production_t);

  for (int i = 0; i < rules.n; i++) {
    rule_def r = rules.rules[i];
    production_t p = {0};
    // Allow skipping productions.
    // This is useful if rules are backed by an enum
    // and this enum does not start at 0 / has skips
    if (r.id) {
      p.identifier = mk_slice(r.id);
      g.ctx = mk_ctx(r.rule);
      if (!expression(&g, &p.expr))
        die("Failed to parse grammar.");
    }
    vec_push(&g.productions_vec, &p);
  }

  if (!finalize_parser(&g))
    die("Failed to construct parser.");
  return g;
}

parser_t mk_parser_raw(const char *text, scanner *s) {
  parser_t g = {0};
  if (!text)
    return g;
  init_parser(&g, *s);

  g.ctx = mk_ctx(text);
  bool success = syntax(&g);
  if (!success)
    die("Failed to parse grammar.");
  if (!finalize_parser(&g))
    die("Failed to construct parser.");
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
  vec_destroy(&p->header->first_vec);
  vec_destroy(&p->header->follow_vec);
}

void destroy_parser(parser_t *g) {
  if (g) {
    destroy_scanner(g->s);
    v_foreach(production_t *, p, g->productions_vec) { destroy_production(p); }
    vec_destroy(&g->productions_vec);
    destroy_arena(g->a);
  }
}

static bool _parse(header_t *hd, parser_t *g, AST **node) {
  struct parse_frame {
    int source_cursor;
    int token_cursor;
    symbol_t *symbol;
  };

  AST **insert_child;
  int start;
  string_slice name;
  vec alt_stack;
  symbol_t *x;
  bool match;
  parse_context *ctx = g->s->ctx;
  start = ctx->c;

  alt_stack = v_make(struct parse_frame);
  name = hd->prod->identifier;
  *node = mk_ast();
  (*node)->node_id = hd->prod->id;
  insert_child = &(*node)->first_child;

  x = hd->sym;
  while (x) {
    AST *next_child = NULL;
    struct parse_frame frame = {.source_cursor = ctx->c};

    switch (x->type) {
    case error_symbol:
      die("Error symbol ??");
    case empty_symbol:
      match = true;
      break;
    case nonterminal_symbol:
      match = _parse(x->nonterminal->header, g, &next_child);
      break;
    case token_symbol: {
      string_slice content = {0};
      match = match_token(g->s, x->token->id, &content);
      if (match) {
        next_child = mk_ast();
        next_child->name = x->token->name;
        next_child->node_id = x->token->id;
        next_child->range = content;
      }
    } break;
    case string_symbol: {
      string_slice content = {0};
      match = match_slice(g->s, x->string, &content);
      if (match) {
        next_child = mk_ast();
        next_child->node_id = -1;
        next_child->name = x->string;
        next_child->range = (string_slice){.n = x->string.n, .str = ctx->src + g->s->ctx->c};
      }
    } break;
    }

    if (match && x->type != empty_symbol) {
      *insert_child = next_child;
      insert_child = &next_child->next;
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
      struct parse_frame *f = vec_pop(&alt_stack);
      if (f && f->source_cursor == ctx->c)
        x = f->symbol;
    }
  }

  int len = ctx->c - start;
  string_slice range = {.str = ctx->src + start, .n = len};
  if (match) {
    (*node)->range = range;
    (*node)->name = name;
  } else {
    destroy_ast(*node);
    *node = NULL;
  }
  vec_destroy(&alt_stack);
  return match;
}

bool parse(parser_t *g, parse_context *ctx, AST **root, int start_rule) {
  if (root == NULL || g == NULL)
    return false;
  g->s->ctx = ctx;
  production_t *start = &g->productions[start_rule];
  bool success = _parse(start->header, g, root);
  success &= next_token(g->s, NULL, NULL) == EOF_TOKEN;
  return success;
}
