// link ebnf/ebnf.o ebnf/analysis.o scanner/scanner.o
// link regex.o arena.o collections.o logging.o
#include "../unittest.h"
#include "ebnf/ebnf.h"
#include "logging.h"
#include "macros.h"
#include "scanner/scanner.h"
#include "text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct testcase {
  char *src;
  bool expected;
};

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
    parser_t p = mk_parser_raw(test->grammar, &s);
    p.backtrack = true;
    AST *a;
    if (!parse(&p, &mk_ctx("bc"), &a, 0)) {
      printf("Error parsing program %s:\n", "bc");
      error_ctx(s.ctx);
      printf("With grammar %s\n", test->grammar);
    }
    destroy_ast(a);
    destroy_parser(&p);
  }
}

void test_parser2(parser_t *g, int n, struct testcase testcases[static n], enum loglevel l, int start_rule) {
  int ll = set_loglevel(l);
  // this is a bit spammy for failing grammars
  for (int i = 0; i < n; i++) {
    struct testcase *test = &testcases[i];
    AST *a;

    char *truth[] = {"false", "true"};
    bool success = parse(g, &mk_ctx(test->src), &a, start_rule);
    if (success != test->expected) {
      print_ast(a, NULL);
      error("Error parsing program %s: was %s, expected %s\n", test->src, truth[success], truth[test->expected]);
      error_ctx(g->s->ctx);
      exit(1);
    }
    destroy_ast(a);
  }
  set_loglevel(ll);
}

void test_multiple_optionals(void) {
  { // Successive Optionals
    const char grammar[] = {
        "A = [ 'a' ] [ 'b' ] .\n"};
    scanner s = {0};
    parser_t p = mk_parser_raw(grammar, &s);

    struct testcase testcases[] = {
        {"",    true },
        {"a",   true },
        {"b",   true },
        {"ab",  true },
        {"aa",  false},
        {"c",   false},
        {"bc",  false},
        {"bcd", false},
        {"bcd", false},
        {"abb", false},
    };

    test_parser2(&p, LENGTH(testcases), testcases, WARN, 0);

    destroy_scanner(&s);
    destroy_parser(&p);
  }
  { // Nested optionals
    const char grammar[] = {
        "A = [ 'a' ] [ 'b' [ 'c' ] [ 'd' ] ] .\n"};
    scanner s = {0};
    parser_t p = mk_parser_raw(grammar, &s);

    struct testcase testcases[] = {
        {"abb", false},
        {"",    true },
        {"a",   true },
        {"b",   true },
        {"ab",  true },
        {"aa",  false},
        {"c",   false},
        {"bc",  true },
        {"bcd", true },
        {"bcd", true },
    };

    test_parser2(&p, LENGTH(testcases), testcases, WARN, 0);

    destroy_scanner(&s);
    destroy_parser(&p);
  }
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
  parser_t p = mk_parser_raw(grammar, &s);
  test_parser2(&p, LENGTH(testcases), testcases, WARN, 0);
  destroy_parser(&p);
  destroy_scanner(&s);
}

void json_parser(void) {
  enum json_tokens {
    string,
    number,
    boolean,
    comma,
    colon,
    lsqbrk,
    rsqbrk,
    lcbrk,
    rcbrk,
    object,
    list,
    keyvalues,
    keyvalue
  };

#define tok(key, pattern) [key] = {#key, \
                                   (char *)pattern}

  static token_def json_tokens[] = {
      tok(string, string_regex),
      tok(number, "-?(\\d+|\\d+\\.\\d*|\\d*\\.\\d+)"),
      tok(boolean, "true|false"),
      tok(comma, ","),
      tok(colon, ":"),
      tok(lsqbrk, "\\["),
      tok(rsqbrk, "\\]"),
      tok(lcbrk, "{"),
      tok(rcbrk, "}"),
  };
  const struct rule_def rules[] = {
      tok(object, "( lcbrk keyvalues rcbrk | lsqbrk list rsqbrk | number | string | boolean )"),
      tok(list, "[ object { comma object } ] "),
      tok(keyvalues, "[ keyvalue { comma keyvalue } ]"),
      tok(keyvalue, "string colon object"),
  };
#undef tok

  scanner s = {0};
  mk_scanner(&s, LENGTH(json_tokens), json_tokens);

  parser_t p = mk_parser(LENGTH(rules), rules, &s);

  if (!is_ll1(&p)) {
    die("Expected json to be ll1");
  }

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

  test_parser2(&p, LENGTH(testcases), testcases, DEBUG, object);

  {
    AST *a;
    if (!parse(&p, &mk_ctx(" 1 "), &a, object)) {
      die("Failed to parse %.*s as object", p.s->ctx->n, p.s->ctx->src);
    }
    assert2(a->node_id == object);
    assert2(!a->next);
    assert2(0 == slicecmp(a->range, mk_slice(" 1 ")));
    AST *chld = a->first_child;
    assert2(chld != NULL);
    assert2(chld->node_id == number);
    assert2(0 == slicecmp(chld->range, mk_slice("1")));
    destroy_ast(a);
  }

  {
    AST *a;
    if (!parse(&p, &mk_ctx("\"a\":\"b\""), &a, keyvalue)) {
      die("Failed to parse %.*s as keyvalue", p.s->ctx->n, p.s->ctx->src);
    }
    assert2(a->node_id == keyvalue);
    assert2(!a->next);
    assert2(0 == slicecmp(a->range, mk_slice("\"a\":\"b\"")));
    AST *chld = a->first_child;
    assert2(chld->node_id == string);
    assert2(chld->first_child == NULL);
    chld = chld->next;
    assert2(chld->node_id == colon);
    assert2(chld->first_child == NULL);
    chld = chld->next;
    assert2(chld->node_id == object);
    assert2(chld->next == NULL);
    chld = chld->first_child;
    assert2(chld->node_id == string);
    assert2(chld->first_child == NULL);
    assert2(0 == slicecmp(chld->range, mk_slice("\"b\"")));
    destroy_ast(a);
  }
  destroy_parser(&p);
  destroy_scanner(&s);
}

void test_ll12(bool expected, const char *grammar, scanner *s) {
  scanner s2 = {0};
  parser_t p = mk_parser_raw(grammar, s ? s : &s2);
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
          {"X", "x*"}
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
      destroy_scanner(&s);
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
  parser_t p = mk_parser_raw(grammar, &s);
  test_parser2(&p, LENGTH(testcases), testcases, WARN, 0);
  destroy_parser(&p);
}

void test_calculator(void) {
  token_def tokens[] = {
      {"number", "-?\\d+"}
  };
  static const char calc_grammar[] = {
      "expression = term {('+' | '-' ) term } .\n"
      "term       = factor {('*' | '/') factor } .\n"
      "factor     = ( digits | '(' expression ')' ) .\n"
      "digits     = number .\n"
      ""};
  scanner s = {0};
  mk_scanner(&s, LENGTH(tokens), tokens);
  parser_t p = mk_parser_raw(calc_grammar, &s);
  struct testcase testcases[] = {
      {"1+2*3",   true},
      {"(1+2)*3", true},
  };

  if (!is_ll1(&p)) {
    die("Grammar is not ll1.");
  }

  test_parser2(&p, LENGTH(testcases), testcases, WARN, 0);
  destroy_parser(&p);
  destroy_scanner(&s);
}

int main(void) {
  setup_crash_stacktrace_logger();
  // test_parser();
  test_calculator();
  test_lookahead();
  json_parser();
  test_oberon2();
  test_ll1();
  test_multiple_optionals();
  // test_oberon();
  assert2(log_severity() <= INFO);
  return 0;
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
//
//   tokens t = {0};
//   // parse(&p, "MODULE a; END a.", &t);
//   print_tokens(t);
//   vec_destroy(&t.tokens_vec);
//
//   destroy_parser(&p);
// }
