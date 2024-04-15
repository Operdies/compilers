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

struct testcase {
  char *src;
  bool expected;
};

void test_parser2(parser_t *g, int n, struct testcase testcases[static n]) {
  int ll = set_loglevel(WARN);
  // this is a bit spammy for failing grammars
  for (int i = 0; i < n; i++) {
    tokens t = {0};
    struct testcase *test = &testcases[i];
    bool success = parse(g, test->src, &t);
    if (success != test->expected) {
      print_tokens(t);
      error("Error parsing program %s:\n", test->src);
      error("%s", t.error.error);
    }
    vec_destroy(&t.tokens_vec);
  }
  set_loglevel(ll);
}

void test_parser(void) {
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
  parser_t p = mk_parser(grammar);
  test_parser2(&p, LENGTH(testcases), testcases);
  destroy_parser(&p);
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

void print_map(char map[UINT8_MAX]) {
  for (int sym = 0; sym < UINT8_MAX; sym++) {
    if (map[sym]) {
      if (isgraph(sym))
        printf("%c ", sym);
      else
        printf("0x%x ", sym);
    }
  }
}
static void print_follow_set(vec *v, vec *seen, char map[UINT8_MAX]) {
  if (vec_contains(seen, v))
    return;
  vec_push(seen, v);
  v_foreach(struct follow_t *, sym, (*v)) {
    switch (sym->type) {
    case FOLLOW_SYMBOL: {
      regex_first(sym->regex, map);
      break;
    }
    case FOLLOW_FIRST:
      // printf("First(%.*s) ", sym->prod->identifier.n, sym->prod->identifier.str);
      print_follow_set(&sym->prod->header->first_vec, seen, map);
      break;
    case FOLLOW_FOLLOW:
      // printf("Follow(%.*s) ", sym->prod->identifier.n, sym->prod->identifier.str);
      print_follow_set(&sym->prod->header->follow_vec, seen, map);
      break;
    }
  }
}

static void print_first_sets(parser_t *g) {
  {
    v_foreach(production_t *, p, g->productions_vec) {
      header_t *h = p->header;
      populate_first(g, h);
    }
  }
  v_foreach(production_t *, p, g->productions_vec) {
    header_t *h = p->header;
    char *ident = string_slice_clone(p->identifier);
    printf(" First(%22s) %2c  ", ident, '=');
    vec seen = v_make(vec);
    char map[UINT8_MAX] = {0};
    print_follow_set(&h->first_vec, &seen, map);
    vec_destroy(&seen);
    print_map(map);
    puts("");
    free(ident);
  }
  {
    v_foreach(production_t *, p, g->productions_vec)
        vec_destroy(&p->header->first_vec);
  }
}

static void print_follow_sets(parser_t *g) {
  populate_follow(g);
  v_foreach(production_t *, p, g->productions_vec) {
    header_t *h = p->header;
    // vec follow = populate_maps(p, h->n_follow, h->follow);
    char *ident = string_slice_clone(p->identifier);
    printf("Follow(%22s) %2c  ", ident, '=');
    vec seen = v_make(vec);
    char map[UINT8_MAX] = {0};
    print_follow_set(&h->follow_vec, &seen, map);
    vec_destroy(&seen);
    print_map(map);
    puts("");
    free(ident);
    // vec_destroy(&follow);
  }
  v_foreach((void), p, g->productions_vec)
      vec_destroy(&p->header->follow_vec);
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
      "object       = { space } ( '{' keyvalues '}' | '\\[' list '\\]' | number | string | boolean ) { space } .\n"
      "list         = [ object { comma object } ] .\n"
      "keyvalues    = [ keyvalue { comma keyvalue } ] .\n"
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
  p.backtrack = false;
  struct testcase testcases[] = {
      {"",            false},
      {"[1",          false},
      {"[1,2,45,-3]", true },
      {"{ "
       "\"mythingY\":[1, 2,45,-3], "
       "\"truth\":1, "
       "\"q\":[]"
       "}",
       true                },
  };

  // if (!is_ll1(&p)) {
  //   error("Expected json to be ll1");
  // }

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
        "B = '[a-f]' 'b'.\n"
        "C = '[e-k]' 'c'.\n"};
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
        "digits     =  digit { digit } ['-'] .\n"
        "digit      = '[0-9]' .\n"};
    parser_t p = mk_parser(grammar);
    if (!is_ll1(&p)) {
      error("Expected ll1: \n%s", grammar);
    }
    destroy_parser(&p);
  }
  {
    static const char grammar[] = {
        "dong         = 'a' strong | 'g' string.\n"
        "string       =  '\"' alpha { alpha } '\"' .\n"
        "strong       =  '\"' alpha { alpha } '\"' .\n"
        "alpha        = '[hng]' .\n"};
    parser_t p = mk_parser(grammar);
    print_first_sets(&p);
    print_follow_sets(&p);
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
  vec_destroy(&t.tokens_vec);
  return 0;
}

void test_oberon2(void) {
  static char grammar[] = {
      "B = [ A { A 'x' } ] 'z' .\n" // TODO: 'x' not in follow(A)
      "A = '1' .\n"
      ""};
  struct testcase testcases[] = {
      {"z",    true },
      {"1",    false},
      {"1xz",  false},
      {"11xz", true },
      {"11x",  false},
      {"x",    false},
  };

  parser_t p = mk_parser(grammar);
  // print_first_sets(&p);
  // print_follow_sets(&p);
  test_parser2(&p, LENGTH(testcases), testcases);
  destroy_parser(&p);
}

void test_oberon(void) {
  static char grammar[] = {
      "module               = 'MODULE' ident ';' declarations ['BEGIN' StatementSequence] 'END' ident '\\.' .\n"
      "selector             = {'\\.' ident | '\\[' expression '\\]'}.\n"
      "factor               = ident selector | number | '\\(' expression '\\)' | '~' factor .\n"
      "term                 = factor {('\\*' | 'DIV' | 'MOD' | '&') factor} .\n"
      "SimpleExpression     = ['\\+' | '-'] term { ('\\+' | '-' | 'OR') term} .\n"
      "expression           = SimpleExpression [('=' | '#' | '<' | '<=' | '>' | '>=') SimpleExpression] .\n"
      "assignment           = ident selector ':=' expression .\n"
      "ProcedureCall        = ident selector ActualParameters .\n"
      "statement            = [assignment | ProcedureCall | IfStatement | WhileStatement | RepeatStatement].\n"
      "StatementSequence    = statement {';' statement }.\n"
      "FieldList            = [IdentList ':' type].\n"
      "type                 = ident | ArrayType | RecordType.\n"
      "FPSection            = ['VAR'] IdentList ':' type .\n"
      "FormalParameters     = '\\(' [ FPSection { ';' FPSection } ] '\\)' .\n" // TODO: parentheses not registered in follow(FPSection)
      "ProcedureHeading     = 'PROCEDURE' ident [FormalParameters].\n"
      "ProcedureBody        = declarations ['BEGIN' StatementSequence] 'END' ident.\n"
      "ProcedureDeclaration = ProcedureHeading ';' ProcedureBody .\n"
      "declarations         = ['CONST' {ident '=' expression ';'}]"
      " ['TYPE' {ident '=' type ';'}]"
      " ['VAR' {IdentList ':' type ';'}]"
      " {ProcedureDeclaration ';'} .\n"
      "ident                = letter {letter | digit}.\n"
      "integer              = digit {digit}.\n"
      "number               = integer.\n"
      "digit                = '[0-9]'.\n"
      "letter               = 'ident' .\n"
      "ActualParameters     = '(' [expression { ',' expression }] '\\)' .\n"
      "IfStatement          = 'IF' expression 'THEN' StatementSequence"
      " {'ELSIF' expression 'THEN' StatementSequence}"
      " ['ELSE' StatementSequence] 'END' .\n"
      "WhileStatement       = 'WHILE' expression 'DO' StatementSequence 'END' .\n"
      "RepeatStatement      = 'REPEAT' StatementSequence 'UNTIL' expression.\n"
      "IdentList            = ident {',' ident} .\n"
      "ArrayType            = 'ARRAY' expression 'OF' type.\n"
      "RecordType           = 'RECORD' FieldList { ';' FieldList} 'END'.\n"
      ""};

  parser_t p = mk_parser(grammar);

  // print_first_sets(&p);
  // print_follow_sets(&p);

  tokens t = {0};
  // parse(&p, "MODULE a; END a.", &t);
  print_tokens(t);
  vec_destroy(&t.tokens_vec);

  destroy_parser(&p);
}

int main(void) {
  setup_crash_stacktrace_logger();
  test_parser();
  test_lookahead();
  prev_test();
  json_parser();
  test_oberon2();
  test_oberon();
  // test_ll1();
  return 0;
}
