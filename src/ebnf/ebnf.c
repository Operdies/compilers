#include "ebnf/ebnf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assert.h"
#include "collections.h"
#include "logging.h"
#include "macros.h"
#include "regex.h"
#include "scanner/scanner.h"

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
#define POINT (g->ctx.view.str + g->ctx.c)

#define LETTERS "a-zA-Z_\\-"
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

static void _print_ast(AST *root, vec *parents) {
#define arr ((char *)vbuf.array)
  static vec vbuf = {.sz = sizeof(char)};
  const char fork[] = "├";
  const char angle[] = "└";
  const char dash[] = "──";
  const char pipe[] = "│";

  if (!root)
    return;

  vbuf.n = 0;
  if (vbuf.array == NULL) {
    vec_ensure_capacity(&vbuf, 100);
    atexit_r(vec_destroy, &vbuf);
  }
  vec _marker = v_make(AST);

  if (!parents)
    parents = &_marker;

  for (; root; root = root->next) {
    vbuf.n = 0;
    if (root->range.str) {
      v_foreach(AST, p, (*parents)) vec_write(&vbuf, "%s   ", p->next ? pipe : " ");
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
    _print_ast(root->first_child, parents);
    vec_pop(parents);
  }
  vec_destroy(&_marker);
#undef arr
}

void print_ast(AST *root) { _print_ast(root, NULL); }

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
  if (g->ctx.view.str[g->ctx.c] == literal) {
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
        error("Expected STRING\n");
        return false;
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
        error("Unmatched ')' in factor\n");
        return false;
      }
      break;
    case '[':
      f->type = F_OPTIONAL;
      advance(&g->ctx);
      if (!expression(g, &f->expression))
        return false;
      if (!match_literal(g, ']')) {
        error("Unmatched ']' in factor\n");
        return false;
      }
      break;
    case '{':
      f->type = F_REPEAT;
      advance(&g->ctx);
      if (!expression(g, &f->expression))
        return false;
      if (!match_literal(g, '}')) {
        error("Unmatched '}' in factor\n");
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
  v_foreach(production_t, p, g->productions_vec) {
    if (p->identifier.n == name.n)
      if (slicecmp(p->identifier, name) == 0)
        return p;
  }
  return NULL;
}

token *find_token(parser_t *g, string_slice name) {
  v_foreach(token, t, g->s->tokens) {
    if (t->name.n == name.n)
      if (slicecmp(t->name, name) == 0)
        return t;
  }
  return NULL;
}

static bool init_expressions(parser_t *g, expression_t *expr) {
  v_foreach(term_t, t, expr->terms_vec) {
    v_foreach(factor_t, f, t->factors_vec) {
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
  v_foreach(production_t, p, g->productions_vec) {
    p->id = idx_p;
    if (!init_expressions(g, &p->expr))
      return false;
  }
  return true;
}

#define MKSYM() (symbol_t *)arena_alloc(g->a, 1, sizeof(symbol_t))

struct subgraph {
  symbol_t *head;
  symbol_t *tail;
};

#define assert_invariants(subgraph) \
  {                                 \
    assert((subgraph)->head);       \
    assert((subgraph)->tail);       \
    assert(!(subgraph)->tail->alt); \
  }

static void make_optional(parser_t *g, struct subgraph *out, enum factor_switch type) {
  symbol_t *head = MKSYM();
  symbol_t *tail = MKSYM();
  head->type = tail->type = empty_symbol;

  {  // Create a new start state
    head->next = out->head;
    out->head = head;
  }

  // If repeatable, loop back to start. Otherwise, go to exit
  out->tail->next = type == F_REPEAT ? head : tail;

  {  // Create a new exit state
    head->alt = tail;
    out->tail = tail;
  }
}

static bool expression_symbol(parser_t *g, expression_t *expr, struct subgraph *out);

static bool factor_symbol(parser_t *g, factor_t *factor, struct subgraph *out) {
  assert(out);
  symbol_t s = {0};
  switch (factor->type) {
    case F_OPTIONAL:
    case F_REPEAT:
    case F_PARENS: {
      if (!expression_symbol(g, &factor->expression, out))
        return false;
      assert_invariants(out);
      if (factor->type == F_REPEAT || factor->type == F_OPTIONAL) {
        make_optional(g, out, factor->type);
      }
      assert_invariants(out);
      return true;
    }
    case F_IDENTIFIER: {
      production_t *p = factor->identifier.production;
      if (!p) {
        error("Error: unknown terminal %.*s\n", factor->identifier.name.n, factor->identifier.name.str);
        return false;
      }
      s.type = nonterminal_symbol;
      s.nonterminal = p;
      break;
    }
    case F_STRING: {
      s.type = string_symbol;
      s.string = factor->string;
      break;
    }
    case F_TOKEN: {
      s.type = token_symbol;
      s.token = factor->token;
      break;
    }
    default:
      return false;
  }
  symbol_t *new = MKSYM();
  *new = s;
  out->head = out->tail = new;
  return true;
}

static bool term_symbol(parser_t *g, term_t *term, struct subgraph *out) {
  symbol_t **next_loc = &out->head;
  v_foreach(factor_t, f, term->factors_vec) {
    struct subgraph factors = {0};
    if (!factor_symbol(g, f, &factors))
      return false;
    assert_invariants(&factors);
    *next_loc = factors.head;
    next_loc = &factors.tail->next;
    out->tail = factors.tail;
  }
  assert_invariants(out);
  return true;
}

static bool expression_symbol(parser_t *g, expression_t *expr, struct subgraph *out) {
  assert(out);
  symbol_t *tail = MKSYM();
  tail->type = empty_symbol;
  out->tail = tail;

  symbol_t **alt_loc = &out->head;

  v_foreach(term_t, t, expr->terms_vec) {
    struct subgraph term = {0};
    if (!term_symbol(g, t, &term))
      return false;
    *alt_loc = term.head;
    for (alt_loc = &term.head; (*alt_loc)->alt; alt_loc = &(*alt_loc)->alt)
      ;
    alt_loc = &term.head->alt;
    assert_invariants(&term);
    term.tail->next = tail;
    term.tail = tail;
    assert_invariants(&term);
  }

  assert_invariants(out);
  return true;
}

static bool build_parse_table(parser_t *g) {
  v_foreach(production_t, prod, g->productions_vec) {
    // Not all productions are guaranteed to be populated.
    // This is because we want to support addressing tokens by the index
    // of the production so e.g. a token enum can be constructed.
    // But using an enum for indexing requires the ability to skip values.
    // Here we just ad-hoc guess that if the expression has 0 characters, the production is not populated.
    if (prod->expr.range.n) {
      struct subgraph sym = {0};
      if (!expression_symbol(g, &prod->expr, &sym))
        return false;
      assert_invariants(&sym);
      prod->sym = sym.head;
    }
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

parser_t mk_parser_raw(const char *text, scanner s) {
  parser_t g = {0};
  if (!text)
    return g;
  init_parser(&g, s);

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
  v_foreach(factor_t, f, t->factors_vec) {
    if (f->type == F_OPTIONAL || f->type == F_REPEAT || f->type == F_PARENS)
      destroy_expression(&f->expression);
  }
  vec_destroy(&t->factors_vec);
}

void destroy_expression(expression_t *e) {
  v_foreach(term_t, t, e->terms_vec) { destroy_term(t); }
  vec_destroy(&e->terms_vec);
}

static void destroy_production(production_t *p) {
  destroy_expression(&p->expr);
  vec_destroy(&p->first_vec);
  vec_destroy(&p->follow_vec);
}

void destroy_parser(parser_t *g) {
  if (g) {
    destroy_scanner(g->s);
    v_foreach(production_t, p, g->productions_vec) { destroy_production(p); }
    vec_destroy(&g->productions_vec);
    destroy_arena(g->a);
  }
}

void mark(parser_t *g, parse_context *ctx) {
  (void)g;
  (void)ctx;
}

// TODO: Convert the recursive calls to an emulated stack using growable vecs
// This should allow parsing very deeply nested statements
// 1. Create stack_frame struct. Something like { ret_symbol, cursor_start, production, **first_child }
// 2. At the end of the loop, if `x` is null, pop the call stack.
// 2.a x = matched ? ret_symbol->next : ret_symbol->alt
// 2.b restore previous stack_frame
static bool stack_parse(production_t *hd, parser_t *g, AST **result) {
  struct stack_frame {
    AST *node;
    AST **next_child;
    production_t *prod;
    symbol_t *ret;
    int cursor_start;
    int alt_cursor;
  };

  struct parse_frame {
    int source_cursor;
    int token_cursor;
    symbol_t *symbol;
  };

  symbol_t *x;
  bool match = false;
  parse_context *ctx = g->s->ctx;

  vec call_stack = v_make(struct stack_frame);
  vec alt_stack = v_make(struct parse_frame);

  vec_ensure_capacity(&call_stack, 100);
  struct stack_frame stack_frame = {.next_child = result};

  // Put a dummy symbol at the root so the while loop will immediately create the first call stack frame
  x = &(symbol_t){
      .nonterminal = hd,
      .type = nonterminal_symbol,
  };

  while (x) {
    AST *next_child = NULL;
    struct parse_frame frame = {.source_cursor = ctx->c};

    switch (x->type) {
      case error_symbol:
        die("Error symbol ??");
      case empty_symbol:
        match = true;
        break;
      case nonterminal_symbol: {
        // Resume from the current symbol when the new frame is finished
        stack_frame.ret = x;
        vec_push(&call_stack, &stack_frame);
        AST *subtree = mk_ast();
        stack_frame = (struct stack_frame){
            .prod = x->nonterminal,
            .ret = NULL,
            .node = subtree,
            .next_child = &subtree->first_child,
            .alt_cursor = alt_stack.n,
            .cursor_start = ctx->c,
        };
        x = x->nonterminal->sym;
        continue;
      } break;
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
          next_child->range = (string_slice){.n = x->string.n, .str = ctx->view.str + g->s->ctx->c};
        }
      } break;
    }

    if (match && next_child) {
      *stack_frame.next_child = next_child;
      stack_frame.next_child = &next_child->next;
    }

    {  // pick next state, and potentially store the alt option for later
      // If the 'next' option is used, push a frame so the alt option can be tried instead.
      if (x->alt && match) {
        frame.symbol = x->alt;
        vec_push(&alt_stack, &frame);
      }
      x = match ? x->next : x->alt;
    }

    // If an end symbol was reached without a match, check if a suitable frame can be restored
    if (x == NULL && match == false && alt_stack.n > stack_frame.alt_cursor) {
      struct parse_frame *f = vec_pop(&alt_stack);
      if (f && f->source_cursor == ctx->c)
        x = f->symbol;
    }

    // If x is still null, we are done with this frame.
    // Pop the frame and continue
    while (!x && call_stack.n) {
      struct stack_frame current = stack_frame;
      stack_frame = *(struct stack_frame *)vec_pop(&call_stack);

      int len = ctx->c - current.cursor_start;
      string_slice range = {.str = ctx->view.str + current.cursor_start, .n = len};
      if (match) {
        current.node->node_id = current.prod->id;
        current.node->range = range;
        current.node->name = current.prod->identifier;
        *stack_frame.next_child = current.node;
        stack_frame.next_child = &current.node->next;
      } else {
        destroy_ast(current.node);
      }
      alt_stack.n = current.alt_cursor;

      if (stack_frame.ret) {
        if (match) {
          x = stack_frame.ret->next;
        } else {
          x = stack_frame.ret->alt;
          // If an end symbol was reached without a match, check if a suitable frame can be restored
          if (!x && alt_stack.n > stack_frame.alt_cursor) {
            struct parse_frame *f = vec_pop(&alt_stack);
            if (f && f->source_cursor == ctx->c)
              x = f->symbol;
          }
        }
      }
    }
  }

  if (match) {
    result = stack_frame.next_child;
  } else {
    destroy_ast(stack_frame.node);
    while (call_stack.n) {
      struct stack_frame *f = vec_pop(&call_stack);
      destroy_ast(f->node);
    }
  }

  if (!match && finished(ctx)) {
    debug("Unexpected end of input!");
    debug_ctx(ctx);
  }

  vec_destroy(&alt_stack);
  vec_destroy(&call_stack);
  return match;
}

static bool rec_parse(production_t *hd, parser_t *g, AST **node) {
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
  bool match = false;
  parse_context *ctx = g->s->ctx;
  start = ctx->c;

  alt_stack = v_make(struct parse_frame);
  name = hd->identifier;
  *node = mk_ast();
  (*node)->node_id = hd->id;
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
        match = rec_parse(x->nonterminal, g, &next_child);
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
          next_child->range = (string_slice){.n = x->string.n, .str = ctx->view.str + g->s->ctx->c};
        }
      } break;
    }

    if (match && x->type != empty_symbol) {
      *insert_child = next_child;
      insert_child = &next_child->next;
    }

    {  // pick next state, and potentially store the alt option for later
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
  string_slice range = {.str = ctx->view.str + start, .n = len};
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
  if (root == NULL || g == NULL) {
    warn("Root or parser null");
    return false;
  }
  g->s->ctx = ctx;
  production_t *start = &g->productions[start_rule];
  bool success = g->recursive ? rec_parse(start, g, root) : stack_parse(start, g, root);
  if (success) {
    parse_context copy = *ctx;
    success &= next_token(g->s, NULL, NULL) == EOF_TOKEN;
    if (!success) {
      warn("Parsing stopped here:");
      warn_ctx(&copy);
      destroy_ast(*root);
      *root = NULL;
    }
  }
  return success;
}
