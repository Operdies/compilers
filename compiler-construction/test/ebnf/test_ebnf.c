// link ebnf/ebnf.o ebnf/analysis.o
// link regex.o arena.o collections.o logging.o
#include "ebnf/ebnf.h"
#include "logging.h"
#include "macros.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char program[] = {"12-34+(3*(4+2)-1)/1-23"};

void print_tokens(tokens tok) {
  v_foreach(struct token_t *, t, tok.tokens_vec) {
    printf("Token '%.*s': '%.*s'\n", t->name.n, t->name.str, t->value.n, t->value.str);
  }
}

void test_lookahead(void) {
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
    vec_destroy(&t.tokens_vec);
    destroy_parser(&p);
  }
}

void test_parser(void) {
  struct testcase {
    char *src;
    bool expected;
  };
  const char grammar[] = {
      "expression = term {('\\+' | '-' ) term } .\n"
      "term       = factor {('\\*' | '/') factor } .\n"
      "factor     = ( digits | '\\(' expression '\\)' ) .\n"
      "digits     = digit { opt [ '!' ] hash digit } .\n"
      "opt        = [ '\\?' ] .\n"
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

  int ll = set_loglevel(WARN);
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
    vec_destroy(&t.tokens_vec);
  }
  destroy_parser(&p);
  set_loglevel(ll);
}

static void print_nonterminals(nonterminal_list ntl) {
  vec buf = v_make(char);
  v_foreach(header_t *, h, ntl.nonterminals_vec) {
    production_t *p = h->prod;
    vec_write(&buf, "%.*s, ", p->identifier.n, p->identifier.str);
  }
  info("Nonterminals: %.*s", buf.n, buf.array);
  vec_destroy(&buf);
}
static void print_terminals(terminal_list tl) {
  vec buf = v_make(char);
  for (int i = 0; i < LENGTH(tl.map); i++) {
    if (tl.map[i]) {
      char ch = (char)i;
      if (isgraph(ch))
        vec_write(&buf, "%c, ", ch);
      else
        vec_write(&buf, "%02x, ", (int)ch);
    }
  }
  info("Terminals: %.*s", buf.n, buf.array);
  vec_destroy(&buf);
}

void print_sym(symbol_t *sym) {
  if (sym->empty)
    printf("empty\n");
  else if (sym->is_nonterminal)
    printf("production '%.*s'\n", sym->nonterminal->identifier.n, sym->nonterminal->identifier.str);
  else
    printf("regex '%s'\n", sym->regex->ctx.src);
}

static void print_follow_set(vec *v) {
  v_foreach(struct follow_t *, sym, (*v)) {
    switch (sym->type) {
    case FOLLOW_SYMBOL: {
      char map[255] = {0};
      regex_first(sym->regex, map);
      for (int sym = 0; sym < LENGTH(map); sym++) {
        if (map[sym]) {
          if (isgraph(sym))
            printf("%c, ", sym);
          else
            printf("0x%x, ", sym);
        }
      }
      break;
    }
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
    vec_destroy(&h->first_vec);
  }
}

static void print_follow_sets(parser_t *g) {
  populate_follow(g);
  v_foreach(production_t *, p, g->productions_vec) {
    header_t *h = p->header;
    printf("Follow(%.*s) %*c ", p->identifier.n, p->identifier.str, 15 - p->identifier.n, '=');
    print_follow_set(&h->follow_vec);
    puts("");
    vec_destroy(&h->follow_vec);
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
    "expression = term {('\\+' | '-' ) term } .\n"
    "term       = factor {('\\*' | '/') factor } .\n"
    "factor     = ( digits | '\\(' expression '\\)' ) .\n"
    "digits     = digit { digit } .\n"
    "digit      = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' .\n"};
/* Follow:
 * expression: ')'
 * term: '+' '-' follow(expression)
 * factor: '*' '/' follow(term)
 * Digits: follow(factor)
 * digit: first(digit) follow(digits)
 */

void json_parser(void) {
  struct testcase {
    char *src;
    bool expected;
  };

  static const char json_grammar[] = {
      "object       = { space } ( '{' [ keyvalues ] '}' | '\\[' [ list ] '\\]' | number | string | boolean ) { space } .\n"
      "list         = object { comma object } .\n"
      "keyvalues    = keyvalue { comma keyvalue } .\n"
      "keyvalue     = { space } string colon object  .\n"
      "string       =  '\"' symbol { symbol } '\"' .\n"
      "symbol       = alphanumeric { alphanumeric } .\n"
      "boolean      = 'true' | 'false' .\n"
      "number       = ( digit | '-' ) { digit } .\n"
      "alphanumeric = '[a-zA-Z0-9]' .\n"
      "comma        = { space } ',' { space } .\n"
      "colon        = { space } ':' { space } .\n"
      "space        = ' ' | '\t' | '\n' .\n"
      "digit        = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' .\n"};

  parser_t p = mk_parser(json_grammar);
  struct testcase testcases[] = {
      {"",            false},
      {"[1",          false},
      {"[1,2,45,-3]", true },
      {"{ "
       "  \"mythingY\":[1, 2,45,-3], "
       "  \"truth\":1 "
       "}",
       true                },
  };

  int ll = set_loglevel(WARN);
  for (int i = 0; i < LENGTH(testcases); i++) {
    tokens t = {0};
    struct testcase *test = &testcases[i];
    bool success = parse(&p, test->src, &t);
    if (success != test->expected) {
      error("Error parsing program %s:\n", test->src);
      error("%s", t.error.error);
      exit(1);
    }
    vec_destroy(&t.tokens_vec);
  }
  destroy_parser(&p);
  set_loglevel(ll);
}

void test_ll1(void) {
  {
    static const char grammar[] = {
        "A = B | C.\n"
        "B = 'b' 'b'.\n"
        "C = 'b' 'c'.\n"};
    parser_t p = mk_parser(grammar);
    if (is_ll1(&p)) {
      error("Expected not ll1: \n%s", grammar);
    }
    destroy_parser(&p);
  }
  {
    static const char grammar[] = {
        "expression = term {('\\+' | '-' ) term } .\n"
        "term       = factor {('\\*' | '/') factor } .\n"
        "factor     = ( digits | '\\(' expression '\\)' ) .\n"
        "digits     =  digit { digit } .\n"
        "digit      = '[0-9]' .\n"};
    parser_t p = mk_parser(grammar);
    if (!is_ll1(&p)) {
      die("Expected ll1: \n%s", grammar);
    }
    destroy_parser(&p);
  }
}

int prev_test(void) {

  parser_t p = mk_parser(calc_grammar);
  if (p.n_productions == 0) {
    fprintf(stderr, "Failed to parse grammar.\n");
    return 1;
  }

  tokens t = {0};
  if (!parse(&p, program, &t)) {
    printf("Error parsing program %s:\n", program);
    printf("%s", t.error.error);
    vec_destroy(&t.tokens_vec);
  }

  destroy_parser(&p);
  return 0;
}

int main(void) {
  setup_crash_stacktrace_logger();
  test_parser();
  test_lookahead();
  prev_test();
  json_parser();
  test_ll1();
  return 0;
}
