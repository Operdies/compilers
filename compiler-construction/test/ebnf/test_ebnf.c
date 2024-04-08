// link ebnf/ebnf.o ebnf/analysis.o
// link regex.o arena.o collections.o logging.o
#include "ebnf/ebnf.h"
#include "logging.h"
#include "macros.h"
#include <ctype.h>
#include <stdio.h>

static const char program[] = {"12-34+(3*(4+2)-1)/1-23"};

static void print_tokens(tokens tok) {
  v_foreach(struct token_t *, t, tok.tokens_vec) {
    printf("Token '%.*s': '%.*s'\n", t->name.n, t->name.str, t->value.n, t->value.str);
  }
}

static void test_lookahead(void) {
  struct testcase {
    char *grammar;
    int lookahead;
  };

  struct testcase testcases[] = {
      {
       .lookahead = 1,
       .grammar = "A = { B | C } .\n"
                     "B = 'b' .\n"
                     "C = 'c' .\n",
       },
      {
       .lookahead = 2,
       .grammar = "A = B | C .\n"
                     "B = 'bb' .\n"
                     "C = 'bc' .\n",
       },
  };

  for (int i = 0; i < LENGTH(testcases); i++) {
    struct testcase *test = &testcases[i];
    parser_t p = mk_parser(test->grammar);
    p.backtrack = true;
    tokens t = {0};
    if (!parse(&p, "bc", &t)) {
      printf("Error parsing program %s:\n", "bc");
      printf("%s", t.error.error);
      printf("With grammar %s\n", test->grammar);
    }
  }
}

static void test_parser(void) {
  struct testcase {
    char *src;
    bool expected;
  };
  const char grammar[] = {
      "expression = term {('+' | '-' ) term } .\n"
      "term       = factor {('*' | '/') factor } .\n"
      "factor     = ( digits | '(' expression ')' ) .\n"
      "digits     = digit { opt [ '!' ] hash digit } .\n"
      "opt        = [ '?' ] .\n"
      "hash       = [ '#' ] .\n"
      "digit      = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' .\n"};

  struct testcase testcases[] = {
      {"12?!#1", true },
      {"1?",     false},
      {"",       false},
      {"()",     false},
      {"1?2",    true },
      {"23",     true },
      {"45*67",  true },
      {"1?1",    true },
      {"1+1",    true },
      {"(1+1)",  true },
  };

  parser_t p = mk_parser(grammar);
  for (int i = 0; i < LENGTH(testcases); i++) {
    tokens t = {0};
    struct testcase *test = &testcases[i];
    bool success = parse(&p, test->src, &t);
    if (success != test->expected) {
      print_tokens(t);
      printf("Error parsing program %s:\n", program);
      printf("%s", t.error.error);
    }
  }
}

static void print_nonterminals(nonterminal_list ntl) {
  vec buf = {0};
  buf.sz = sizeof(char);
  v_foreach(header_t *, h, ntl.nonterminals_vec) {
    production_t *p = h->prod;
    vec_write(&buf, "%.*s, ", p->identifier.n, p->identifier.str);
  }
  info("Nonterminals: %.*s", buf.n, buf.array);
  vec_destroy(&buf);
}
static void print_terminals(terminal_list tl) {
  vec buf = {0};
  buf.sz = sizeof(char);
  for (int i = 0; i < tl.n_terminals; i++) {
    char ch = tl.terminals[i];
    if (isgraph(ch))
      vec_write(&buf, "%c, ", ch);
    else
      vec_write(&buf, "%02x, ", (int)ch);
  }
  info("Terminals: %.*s", buf.n, buf.array);
  vec_destroy(&buf);
}

static void print_sym(symbol_t *sym) {
  if (sym->empty)
    printf("empty\n");
  else if (sym->is_nonterminal)
    printf("production '%.*s'\n", sym->nonterminal->identifier.n, sym->nonterminal->identifier.str);
  else if (isgraph(sym->sym))
    printf("symbol '%c'\n", sym->sym);
  else
    printf("symbol 0x%02x\n", (int)sym->sym);
}

static void print_follow_set(vec *v) {
  v_foreach(struct follow_t *, sym, (*v)) {
    switch (sym->type) {
    case FOLLOW_SYMBOL:
      if (isgraph(sym->symbol))
        printf("%c, ", sym->symbol);
      else
        printf("0x%x, ", sym->symbol);
      break;
    case FOLLOW_FIRST:
      printf("First(%.*s), ", sym->prod->identifier.n, sym->prod->identifier.str);
      break;
    case FOLLOW_FOLLOW:
      printf("Follow(%.*s), ", sym->prod->identifier.n, sym->prod->identifier.str);
      break;
    }
  }
}

static void print_first_sets(parser_t *g) {
  v_foreach(production_t *, p, g->productions_vec) {
    header_t *h = p->header;
    populate_first(g, h);
    printf("First(%.*s)  %*c ", p->identifier.n, p->identifier.str, 15 - p->identifier.n, '=');
    print_follow_set(&h->first_vec);
    puts("");
  }
}

static void print_follow_sets(parser_t *g) {
  populate_follow(g);
  v_foreach(production_t *, p, g->productions_vec) {
    header_t *h = p->header;
    printf("Follow(%.*s) %*c ", p->identifier.n, p->identifier.str, 15 - p->identifier.n, '=');
    print_follow_set(&h->follow_vec);
    puts("");
  }
}

static void print_enumerated_graph(vec all) {
  v_foreach(symbol_t *, sym, all) {
    int idx = idx_sym;
    printf("%2d) ", idx);
    print_sym(sym);
  }
}

static const char calc_grammar[] = {
    "expression = term {('+' | '-' ) term } .\n"
    "term       = factor {('*' | '/') factor } .\n"
    "factor     = ( digits | '(' expression ')' ) .\n"
    "digits     = digit { digit } .\n"
    "digit      = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' .\n"};
/* Follow:
 * expression: ')'
 * term: '+' '-' follow(expression)
 * factor: '*' '/' follow(term)
 * Digits: follow(factor)
 * digit: first(digit) follow(digits)
 */

void test_ll1(void) {
  static const char grammar[] = {
      "expression = term {('+' | '-' ) term } .\n"
      "term       = factor {('*' | '/') factor } .\n"
      "factor     = ( digits | '(' expression ')' ) .\n"
      "digits     = digit { digit } .\n"
      "digit      = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' .\n"};
  parser_t p = mk_parser(grammar);
  info("First Set");
  print_first_sets(&p);
  info("Follow Set");
  print_follow_sets(&p);
  terminal_list t = get_terminals(&p);
  nonterminal_list nt = get_nonterminals(&p);
  print_terminals(t);
  print_nonterminals(nt);
  destroy_parser(&p);
}

static int prev_test(void) {

  parser_t p = mk_parser(calc_grammar);
  if (p.n_productions == 0) {
    fprintf(stderr, "Failed to parse grammar.\n");
    return 1;
  }

  tokens t = {0};
  if (!parse(&p, program, &t)) {
    printf("Error parsing program %s:\n", program);
    printf("%s", t.error.error);
  }

  destroy_parser(&p);
  return 0;
}

int main(void) {
  setup_crash_stacktrace_logger();
  test_parser();
  test_lookahead();
  prev_test();
  test_ll1();
  return 0;
}
