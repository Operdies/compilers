// link ebnf/ebnf.o ebnf/analysis.o scanner/scanner.o
// link regex.o arena.o collections.o logging.o
#include "ebnf/ebnf.h"
#include "logging.h"
#include "macros.h"
#include "text.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_tokens(tokens tok) {
  v_foreach(struct token_t *, t, tok.tokens_vec) {
    info("%3d Token '%.*s'\n%.*s'\n", idx_t, t->name.n, t->name.str, t->value.n, t->value.str);
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
    scanner s = {0};
    parser_t p = mk_parser(test->grammar, &s);
    p.backtrack = true;
    AST *a;
    s.ctx = &mk_ctx("bc");
    if (!parse(&p, &a)) {
      printf("Error parsing program %s:\n", "bc");
      error_ctx(s.ctx);
      printf("With grammar %s\n", test->grammar);
    }
    destroy_ast(a);
    destroy_parser(&p);
  }
}

struct testcase {
  char *src;
  bool expected;
};

void print_scanner(scanner *s) {
  s->ctx->c = 0;
  int tok;
  string_slice slc;
  while ((tok = next_token(s, NULL, &slc)) >= 0) {
    token *t = vec_nth(&s->tokens.slice, tok);
    debug("Token: %.*s %.*s", t->name.n, t->name.str, slc.n, slc.str);
  }
  s->ctx->c = 0;
}

void test_parser2(parser_t *g, int n, struct testcase testcases[static n], enum loglevel l) {
  int ll = set_loglevel(l);
  // this is a bit spammy for failing grammars
  for (int i = 0; i < n; i++) {
    struct testcase *test = &testcases[i];
    AST *a;
    parse_context ctx = {.src = test->src, .n = strlen(test->src)};
    g->s->ctx = &ctx;

    bool success = parse(g, &a);
    if (success != test->expected) {
      print_ast(a, NULL);
      error("Error parsing program %s:\n", test->src);
      error_ctx(&ctx);
      exit(1);
    }
    destroy_ast(a);
  }
  set_loglevel(ll);
}

void test_parser(void) {
  const char grammar[] = {
      "expression = term {('+' | '-' ) term } .\n"
      "term       = factor {('*' | '/') factor } .\n"
      "factor     = ( digits | '(' expression ')' ) .\n"
      "digits     = digit { opt [ '!' ] hash digit } .\n"
      "opt        = [ '?' ] .\n"
      "hash       = [ '#' ] .\n"
      "digit      = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' .\n"
      ""};

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
  scanner s = {0};
  parser_t p = mk_parser(grammar, &s);
  test_parser2(&p, LENGTH(testcases), testcases, WARN);
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
  const char *s = describe_symbol(sym);
  info(s);
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
    case FOLLOW_CHAR:
      map[(int)sym->ch] = 1;
      break;
    }
  }
}

static void print_first_sets(parser_t *g) {
  {
    v_foreach(production_t *, p, g->productions_vec) {
      header_t *h = p->header;
      populate_first(h);
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
}

static void print_enumerated_graph(vec all) {
  v_foreach(symbol_t *, sym, all) {
    int idx = idx_sym;
    printf("%2d) ", idx);
    print_sym(sym);
  }
}

void json_parser(void) {
  static token_def json_tokens[] = {
      {(char *)string_regex,               "string"  },
      {"-?(\\d+|\\d+\\.\\d*|\\d*\\.\\d+)", "number"  },
      {"true|false",                       "boolean" },
      {",",                                "comma"   },
      {":",                                "colon"   },
      {"\\[",                              "lbrace"  },
      {"\\]",                              "rbrace"  },
      {"{",                                "lbracket"},
      {"}",                                "rbracket"},
  };
  const char json_grammar[] = {
      "object       = ( lbracket keyvalues rbracket | lbrace list rbrace | number | string | boolean ) .\n"
      "list         = [ object { comma object } ] .\n"
      "keyvalues    = [ keyvalue { comma keyvalue } ] .\n"
      "keyvalue     = string colon object  .\n"
      ""};

  scanner s = {0};
  mk_scanner(&s, LENGTH(json_tokens), json_tokens);

  parser_t p = mk_parser(json_grammar, &s);
  struct testcase testcases[] = {
      {"",                   false},
      {"[1",                 false},
      {"[1,2,45,-3]",        true },
      {"[1 , 2 , 45 , -3 ]", true },
      {"{\"a\":1}",          true },
      {"{"
       "\"key one\": [1,2,45,-3],"
       "\"number\":1,"
       "\"obj\":{ \"v\": \"str\"}"
       "}",
       true                       },
  };

  if (!is_ll1(&p)) {
    die("Expected json to be ll1");
  }

  test_parser2(&p, LENGTH(testcases), testcases, DEBUG);
  destroy_parser(&p);
  destroy_scanner(&s);
}

void test_ll12(bool expected, const char *grammar, scanner *s) {
  scanner s2 = {0};
  parser_t p = mk_parser(grammar, s ? s : &s2);
  int ll = 0;
  if (!expected)
    ll = set_loglevel(WARN);
  if (is_ll1(&p) != expected) {
    error("Expected %sll1: \n%s", expected ? " " : "not ", grammar);
  }
  destroy_parser(&p);
  if (ll)
    set_loglevel(ll);
}

void test_ll1(void) {
  {
    test_ll12(true, "dong         = 'a' strong | 'g' string.\n"
                    "string       =  '\"' alpha { alpha } '\"' .\n"
                    "strong       =  '\"' alpha { alpha } '\"' .\n"
                    "alpha        = 'h' | 'n' | 'g' .\n",
              NULL);
  }
  {
    // 1. term0 | term1    -> the terms must not have any common start symbols
    test_ll12(true,
              "A = B | C.\n"
              "B = 'b'.\n"
              "C = 'c'.\n",
              NULL);
    test_ll12(false,
              "A = B | C.\n"
              "B = 'b'.\n"
              "C = 'b'.\n",
              NULL);
    test_ll12(true, "A = 'b' | 'c'.",
              NULL);
    test_ll12(false, "A = 'bc' | 'bb'.",
              NULL);
  }
  {
    // 2. fac0 fac1        -> if fac0 contains the empty sequence, then the factors must not have any common start symbols
    test_ll12(true, "A = 'b' 'b'.",
              NULL);
    test_ll12(false, "A = [ 'b' ] 'b'.",
              NULL);
    test_ll12(true, "A = B 'b'.\n"
                    "B = [ 'a' ] { 'd' }.",
              NULL);
    test_ll12(false, "A = B 'b'.\n"
                     "B = 'a' { 'b' }.",
              NULL);
    test_ll12(false, "A = B 'b'.\n"
                     "B = [ 'a' ] { 'b' }.",
              NULL);
  }

  {
    // 3 [exp] or {exp}    -> the sets of start symbols of exp and of symbols that may follow K must be disjoint

    { // scenario 1: term ends with an optional
      test_ll12(false,
                "A = B 'x' .\n"
                "B = 'b' { 'x' } .",
                NULL);
      test_ll12(false,
                "A = B 'x' .\n"
                "B = 'b' [ 'x' ] .",
                NULL);
      test_ll12(false,
                "A = B 'x' .\n"
                "B = 'b' { [ 'x' ] } .",
                NULL);
      test_ll12(true,
                "A = B 'x' .\n"
                "B = 'b' { [ 'x' ] } 'x' .",
                NULL);
      test_ll12(true,
                "A = B 'x' .\n"
                "B = 'b' 'x' .",
                NULL);
      test_ll12(false,
                "A = B 'x' .\n"
                "B = { 'x' } .",
                NULL);
    }

    { // scenario 2: term ends with a production which contains the empty set
      test_ll12(false,
                "A = B 'x' .\n"
                "B = 'a' C .\n"
                "C = { 'x' } .",
                NULL);
      test_ll12(true,
                "A = B 'x' .\n"
                "B = 'a' C .\n"
                "C = 'x' { 'y' } 'x' .",
                NULL);
    }

    { // scenario 3: term ends with a regex which can match the empty set
      scanner s = {0};
      token_def tokens[] = {
          {"x*", "X"}
      };
      mk_scanner(&s, 1, tokens);
      test_ll12(false,
                "A = B 'x' .\n"
                "B = 'a' X .\n",
                &s);
      test_ll12(true,
                "A = B 'x' .\n"
                "B = 'a' X 'x' .\n",
                &s);

      // scenario 3.b: regex can end with repeating a character from the first set
      // test_ll12(false,
      //           "A = B 'x' .\n"
      //           "B = 'a' 'x+' .\n");
    }
  }
  {
    // 4 ?? parenthesized expressions are a bit trickier.
    // We probably need to treat all expressions as productions, in an ideal world
    // example A = ( 'a' [ 'b' ]) { 'b' }
    // This is similar to rule 2.
    // 2. fac0 fac1        -> if fac0 contains the empty sequence, then the factors must not have any common start symbols
    // Clearly, ( 'a' [ 'b' ]) does not contain the empty sequence, and yet there is a problem.
    // Do we need to recurse into the parenthesized expression and check each term to see if their terminating factors can be empty?
  }
  {
    static const char grammar[] = {
        "A = B | C.\n"
        "B = '[a-f]' 'b'.\n"
        "C = '[e-k]' 'c'.\n"
        ""};
    test_ll12(false, grammar, NULL);
  }
  {
    static const char grammar[] = {
        "expression = term {('+' | '-' ) term } .\n"
        "term       = factor {('*' | '/') factor } .\n"
        "factor     = ( digits | '(' expression ')' ) .\n"
        "digits     = ['-'] '[0-9]+' .\n"
        ""};
    test_ll12(true, grammar, NULL);
  }
}

void test_oberon2(void) {
  static char grammar[] = {
      "B = [ A { A 'x' } ] 'z' .\n"
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

  scanner s = {0};
  parser_t p = mk_parser(grammar, &s);
  // print_first_sets(&p);
  // print_follow_sets(&p);
  test_parser2(&p, LENGTH(testcases), testcases, WARN);
  destroy_parser(&p);
}

// TODO: update this test.
// 1. Define tokens for use in a parser
// void test_oberon(void) {
//   static char grammar[] = {
//       "module               = 'MODULE' ident ';' declarations ['BEGIN' StatementSequence] 'END' ident '\\.' .\n"
//       "selector             = {'\\.' ident | '\\[' expression '\\]'}.\n"
//       "factor               = ident selector | number | '\\(' expression '\\)' | '~' factor .\n"
//       "term                 = factor {('\\*' | 'DIV' | 'MOD' | '&') factor} .\n"
//       "SimpleExpression     = ['\\+' | '-'] term { ('\\+' | '-' | 'OR') term} .\n"
//       "expression           = SimpleExpression [('=' | '#' | '<' | '<=' | '>' | '>=') SimpleExpression] .\n"
//       "assignment           = ident selector ':=' expression .\n"
//       "ProcedureCall        = ident selector ActualParameters .\n"
//       "statement            = [assignment | ProcedureCall | IfStatement | WhileStatement | RepeatStatement].\n"
//       "StatementSequence    = statement {';' statement }.\n"
//       "FieldList            = [IdentList ':' type].\n"
//       "type                 = ident | ArrayType | RecordType.\n"
//       "FPSection            = ['VAR'] IdentList ':' type .\n"
//       "FormalParameters     = '\\(' [ FPSection { ';' FPSection } ] '\\)' .\n"
//       "ProcedureHeading     = 'PROCEDURE' ident [FormalParameters].\n"
//       "ProcedureBody        = declarations ['BEGIN' StatementSequence] 'END' ident.\n"
//       "ProcedureDeclaration = ProcedureHeading ';' ProcedureBody .\n"
//       "declarations         = ['CONST' {ident '=' expression ';'}]"
//       " ['TYPE' {ident '=' type ';'}]"
//       " ['VAR' {IdentList ':' type ';'}]"
//       " {ProcedureDeclaration ';'} .\n"
//       "ident                = letter {letter | digit}.\n"
//       "integer              = digit {digit}.\n"
//       "number               = integer.\n"
//       "digit                = '[0-9]'.\n"
//       "letter               = 'ident' .\n"
//       "ActualParameters     = '(' [expression { ',' expression }] '\\)' .\n"
//       "IfStatement          = 'IF' expression 'THEN' StatementSequence"
//       " {'ELSIF' expression 'THEN' StatementSequence}"
//       " ['ELSE' StatementSequence] 'END' .\n"
//       "WhileStatement       = 'WHILE' expression 'DO' StatementSequence 'END' .\n"
//       "RepeatStatement      = 'REPEAT' StatementSequence 'UNTIL' expression.\n"
//       "IdentList            = ident {',' ident} .\n"
//       "ArrayType            = 'ARRAY' expression 'OF' type.\n"
//       "RecordType           = 'RECORD' FieldList { ';' FieldList} 'END'.\n"
//       ""};
//
//   parser_t p = mk_parser(grammar);
//
//   if (!is_ll1(&p)) {
//     error("Expected ll1: \n%s", grammar);
//   }
//   // print_first_sets(&p);
//   // print_follow_sets(&p);
//
//   tokens t = {0};
//   // parse(&p, "MODULE a; END a.", &t);
//   print_tokens(t);
//   vec_destroy(&t.tokens_vec);
//
//   destroy_parser(&p);
// }

void test_calculator(void) {
  token_def tokens[] = {
      {"-?\\d+", "number"},
  };
  static const char calc_grammar[] = {
      "expression = term {('+' | '-' ) term } .\n"
      "term       = factor {('*' | '/') factor } .\n"
      "factor     = ( digits | '(' expression ')' ) .\n"
      "digits     = number .\n"
      ""};
  scanner s = {0};
  mk_scanner(&s, LENGTH(tokens), tokens);
  parser_t p = mk_parser(calc_grammar, &s);
  struct testcase testcases[] = {
      {"1+2*3",   true},
      {"(1+2)*3", true},
  };

  if (!is_ll1(&p)) {
    die("Grammar is not ll1.");
  }

  test_parser2(&p, LENGTH(testcases), testcases, WARN);
  destroy_parser(&p);
}

int main(void) {
  setup_crash_stacktrace_logger();
  test_parser();
  test_calculator();
  test_lookahead();
  json_parser();
  test_oberon2();
  // test_oberon();
  test_ll1();
  return 0;
}
