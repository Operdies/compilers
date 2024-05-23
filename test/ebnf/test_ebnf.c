// link ebnf/ebnf.o ebnf/analysis.o scanner/scanner.o
// link regex.o arena.o collections.o logging.o
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../unittest.h"
#include "ebnf/ebnf.h"
#include "logging.h"
#include "macros.h"
#include "scanner/scanner.h"
#include "text.h"

#define tok(key, pattern) [key] = {#key, (char *)pattern}

struct testcase {
  char *src;
  bool expected;
};

static const scanner no_scanner = {0};
static const scanner_tokens no_tokens = {0};
static void AstCmp(AST *left, AST *right);

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
                     "C = 'c' .\n", },
      {
       .lookahead = 2,
       .grammar = "A = B | C .\n"
                     "B = 'bb' .\n"
                     "C = 'bc' .\n", },
  };

  for (int i = 0; i < LENGTH(testcases); i++) {
    struct testcase *test = &testcases[i];
    parser_t p = mk_parser_raw(test->grammar, no_scanner);
    AST *a;
    if (!parse(&p, &mk_ctx("bc"), &a, 0)) {
      printf("Error parsing program %s:\n", "bc");
      error_ctx(p.s->ctx);
      printf("With grammar %s\n", test->grammar);
    }
    destroy_ast(a);
    destroy_parser(&p);
  }
}

void test_parser2(parser_t *g, int n, struct testcase testcases[static n], enum loglevel l, int start_rule) {
  int ll = set_loglevel(l);
  // this is a bit spammy for failing grammars
  // TODO: move diagnostic output into error list / AST so parsers can give specialized errors
  for (int i = 0; i < n; i++) {
    struct testcase *test = &testcases[i];
    AST *a;

    char *truth[] = {"false", "true"};
    bool success = parse(g, &mk_ctx(test->src), &a, start_rule);
    if (success != test->expected) {
      error("Error parsing program %s: was %s, expected %s\n", test->src, truth[success], truth[test->expected]);
      error_ctx(g->s->ctx);
    }
    if (success)
      destroy_ast(a);
  }
  set_loglevel(ll);
}

void test_simplest(void) {
  enum tokens { A, B };

  {
    rule_def rules[] = {tok(A, "'a'")};
    parser_t p = mk_parser(mk_rules(rules), no_tokens, NULL);
    struct testcase testcases[] = {
        {"",   false},
        {"a",  true },
        {"aa", false},
    };
    test_parser2(&p, LENGTH(testcases), testcases, LL_ERROR, 0);
    destroy_parser(&p);
  }

  {
    rule_def rules[] = {tok(A, "'a' { B } 'a'"), tok(B, "'b'")};
    parser_t p = mk_parser(mk_rules(rules), no_tokens, NULL);
    struct testcase testcases[] = {
        {"abba", true },
        {"abb",  false},
        {"",     false},
        {"aa",   true },
    };
    test_parser2(&p, LENGTH(testcases), testcases, LL_ERROR, 0);
    destroy_parser(&p);
  }

  {
    rule_def rules[] = {tok(A, "'a' B 'a'"), tok(B, "'b' 'b'")};
    parser_t p = mk_parser(mk_rules(rules), no_tokens, NULL);
    struct testcase testcases[] = {
        {"",     false},
        {"aa",   false},
        {"abba", true },
    };
    test_parser2(&p, LENGTH(testcases), testcases, LL_ERROR, 0);
    destroy_parser(&p);
  }
}

void test_multiple_optionals(void) {
  {  // Successive Optionals
    const char grammar[] = {"A = [ 'a' ] [ 'b' ] .\n"};
    parser_t p = mk_parser_raw(grammar, no_scanner);

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

    test_parser2(&p, LENGTH(testcases), testcases, LL_ERROR, 0);

    destroy_parser(&p);
  }
  {  // Nested optionals
    const char grammar[] = {"A = [ 'a' ] [ 'b' [ 'c' ] [ 'd' ] ] .\n"};
    parser_t p = mk_parser_raw(grammar, no_scanner);

    struct testcase testcases[] = {
        {"abb",  false},
        {"",     true },
        {"a",    true },
        {"b",    true },
        {"ab",   true },
        {"aa",   false},
        {"c",    false},
        {"bc",   true },
        {"bcd",  true },
        {"abcd", true },
    };

    test_parser2(&p, LENGTH(testcases), testcases, LL_ERROR, 0);

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
      "digit      = '0' | '1' | '2' | '3' | '4' | '5' | "
      "'6' | '7' | '8' | '9' .\n"
      ""};

  struct testcase testcases[] = {
      {"(1+1)",  true },
      {"12?!#1", true },
      {"1?",     false},
      {"",       false},
      {"()",     false},
      {"1?2",    true },
      {"23",     true },
      {"45*67",  true },
      {"1?1",    true },
      {"1+1",    true },
  };
  parser_t p = mk_parser_raw(grammar, no_scanner);
  test_parser2(&p, LENGTH(testcases), testcases, LL_ERROR, 0);
  destroy_parser(&p);
}

void test_repeat(void) {
  enum rules { A, B };
  const rule_def rules[] = {
      tok(A, "B { B }"),
      tok(B, "'a' | 'c'"),
  };
  parser_t p = mk_parser(mk_rules(rules), no_tokens, NULL);
  struct testcase testcases[] = {
      {"a",   true },
      {"",    false},
      {"aa",  true },
      {"b",   false},
      {"ab",  false},
      {"aba", false},
      {"aaa", true },
      {"aab", false},
  };
  test_parser2(&p, LENGTH(testcases), testcases, LL_ERROR, A);
  destroy_parser(&p);
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
  const rule_def rules[] = {
      tok(object,
          "( lcbrk keyvalues rcbrk | lsqbrk list rsqbrk | number | "
          "string | boolean )"),
      tok(list, "[ object { comma object } ] "),
      tok(keyvalues, "[ keyvalue { comma keyvalue } ]"),
      tok(keyvalue, "string colon object"),
  };

  parser_t p = mk_parser(mk_rules(rules), mk_tokens(json_tokens), NULL);

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
       "}",           true },
  };

  test_parser2(&p, LENGTH(testcases), testcases, LL_ERROR, object);

  {
    AST *a;
    if (!parse(&p, &mk_ctx(" 1 "), &a, object)) {
      die("Failed to parse %S as object", p.s->ctx->view);
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
      die("Failed to parse %S as keyvalue", p.s->ctx->view);
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
}

void test_ll12(bool expected, grammar_rules rules, scanner_tokens tokens) {
  parser_t p = mk_parser(rules, tokens, NULL);
  int ll = 0;
  if (!expected)
    ll = set_loglevel(LL_ERROR);
  if (is_ll1(&p) != expected) {
    error("Expected %sll1", expected ? " " : "not ");
  }
  destroy_parser(&p);
  if (ll)
    set_loglevel(ll);
}

void test_ll1(void) {
  enum { A, B, C };
  {
    enum { dong, string, strong, alpha };
    rule_def rules[] = {
        tok(dong, "'a' strong | 'g' string"),
        tok(string, "'\"' alpha { alpha } '\"'"),
        tok(strong, "'\"' alpha { alpha } '\"'"),
        tok(alpha, "'h' | 'n' | 'g'"),
    };
    test_ll12(true, mk_rules(rules), no_tokens);
  }
  {
    // 1. term0 | term1    -> the terms must not have any common start symbols
    rule_def rules[] = {
        tok(A, "B | C"),
        tok(B, "'b'"),
        tok(C, "'c'"),
    };
    test_ll12(true, mk_rules(rules), no_tokens);
    rule_def rules2[] = {
        tok(A, "B | C"),
        tok(B, "'b'"),
        tok(C, "'b'"),
    };
    test_ll12(false, mk_rules(rules2), no_tokens);

    rule_def rules3[] = {tok(A, "'b' | 'c'")};
    test_ll12(true, mk_rules(rules3), no_tokens);
    rule_def rules4[] = {tok(A, "'bc' | 'bb'")};
    test_ll12(false, mk_rules(rules4), no_tokens);
  }
  {
    // 2. fac0 fac1        -> if fac0 contains the empty sequence, then the
    // factors must not have any common start symbols
    {
      rule_def rules[] = {tok(A, "'b' 'b'")};
      test_ll12(true, mk_rules(rules), no_tokens);
    }
    {
      rule_def rules[] = {tok(A, "[ 'b' ] 'b' ")};
      test_ll12(false, mk_rules(rules), no_tokens);
      {
        rule_def rules[] = {
            tok(A, "B 'b'"),
            tok(B, "[ 'a' ] { 'd' }"),
        };
        test_ll12(true, mk_rules(rules), no_tokens);
      }
      {
        rule_def rules[] = {
            tok(A, "B 'b'"),
            tok(B, "'a' { 'b' }"),
        };
        test_ll12(false, mk_rules(rules), no_tokens);
      }
      {
        rule_def rules[] = {
            tok(A, "B 'b'"),
            tok(B, "[ 'a' ] { 'b' }"),
        };
        test_ll12(false, mk_rules(rules), no_tokens);
      }
    }

    {
      // 3 [exp] or {exp}    -> the sets of start symbols of exp and of symbols
      // that may follow K must be disjoint

      {  // scenario 1: term ends with an optional
        {
          rule_def rules[] = {
              tok(A, "B 'x'"),
              tok(B, "'b' { 'x' }"),
          };
          test_ll12(false, mk_rules(rules), no_tokens);
        }
        {
          rule_def rules[] = {
              tok(A, "B 'x'"),
              tok(B, "'b' [ 'x' ]"),
          };
          test_ll12(false, mk_rules(rules), no_tokens);
        }
        {
          rule_def rules[] = {
              tok(A, "B 'x'"),
              tok(B, "'b' { [ 'x' ] }"),
          };
          test_ll12(false, mk_rules(rules), no_tokens);
        }
        {
          rule_def rules[] = {
              tok(A, "B 'x'"),
              tok(B, "'b' { [ 'x' ] } 'x' "),
          };
          test_ll12(true, mk_rules(rules), no_tokens);
        }
        {
          rule_def rules[] = {
              tok(A, "B 'x'"),
              tok(B, "'b' 'x' "),
          };
          test_ll12(true, mk_rules(rules), no_tokens);
        }
        {
          rule_def rules[] = {
              tok(A, "B 'x'"),
              tok(B, "{ 'x' } "),
          };
          test_ll12(false, mk_rules(rules), no_tokens);
        }
      }

      {  // scenario 2: term ends with a production which contains the empty set
        {
          rule_def rules[] = {
              tok(A, "B 'x'"),
              tok(B, "'a' C"),
              tok(C, "{ 'x' }"),
          };
          test_ll12(false, mk_rules(rules), no_tokens);
        }
        {
          rule_def rules[] = {
              tok(A, "B 'x'"),
              tok(B, "'a' C"),
              tok(C, "'x' { 'y' } 'x'"),
          };
          test_ll12(true, mk_rules(rules), no_tokens);
        }
      }

      {  // scenario 3: term ends with a regex which can match the empty set
        token_def tokens[] = {
            {"X", "x*"}
        };
        {
          rule_def rules[] = {
              tok(A, "B 'x'"),
              tok(B, "'a' X"),
          };
          test_ll12(false, mk_rules(rules), mk_tokens(tokens));
        }
        {
          rule_def rules[] = {
              tok(A, "B 'x'"),
              tok(B, "'a' X 'x'"),
          };
          test_ll12(true, mk_rules(rules), mk_tokens(tokens));
        }

        // scenario 3.b: regex can end with repeating a character from the first
        // set test_ll12(false,
        //           "A = B 'x' .\n"
        //           "B = 'a' 'x+' .\n");
      }
    }
    {
      // 4 ?? parenthesized expressions are a bit trickier.
      // We probably need to treat all expressions as productions, in an ideal
      // world example A = ( 'a' [ 'b' ]) { 'b' } This is similar to rule 2.
      // 2. fac0 fac1        -> if fac0 contains the empty sequence, then the
      // factors must not have any common start symbols Clearly, ( 'a' [ 'b' ])
      // does not contain the empty sequence, and yet there is a problem. Do we
      // need to recurse into the parenthesized expression and check each term
      // to see if their terminating factors can be empty?
    }
    {
      rule_def rules[] = {
          tok(A, "B | C"),
          tok(B, "('a' | 'b' | 'c' | 'd' | 'e' | 'f') 'b'"),
          tok(C, "('e' | 'f' | 'g' | 'h' | 'i' | 'j') 'c'"),
      };
      test_ll12(false, mk_rules(rules), no_tokens);
    }
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
  parser_t p = mk_parser_raw(grammar, s);
  test_parser2(&p, LENGTH(testcases), testcases, LL_ERROR, 0);
  destroy_parser(&p);
}

void test_calculator(void) {
  enum productions {
    expression,
    term,
    factor,
    digits,
    number,
  };

  token_def tokens[] = {
      tok(number, "-?\\d+"),
  };

  const rule_def rules[] = {
      tok(expression, "term {('+' | '-') term }"),
      tok(term, "factor {('*' | '/') factor }"),
      tok(factor, "digits | '(' expression ')'"),
      tok(digits, "number"),
  };

  parser_t p = mk_parser(mk_rules(rules), mk_tokens(tokens), NULL);
  struct testcase testcases[] = {
      {"1+2*3",   true},
      {"(1+2)*3", true},
  };

  if (!is_ll1(&p)) {
    die("Grammar is not ll1.");
  }

  test_parser2(&p, LENGTH(testcases), testcases, LL_ERROR, 0);
  destroy_parser(&p);
}

// TODO: update this test.
// 1. Define tokens for use in a parser
void test_oberon(void) {
  enum oberon_tokens {
    module,
    selector,
    factor,
    term,
    SimpleExpression,
    expression,
    assignment,
    ProcedureCall,
    statement,
    StatementSequence,
    FieldList,
    type,
    FPSection,
    FormalParameters,
    ProcedureHeading,
    ProcedureBody,
    ProcedureDeclaration,
    declarations,
    ident,
    integer,
    number,
    ActualParameters,
    IfStatement,
    WhileStatement,
    RepeatStatement,
    IdentList,
    ArrayType,
    RecordType,
    Geq,
    Leq,
  };

  static token_def tokens[] = {
      tok(integer, "[0-9]+"),
      tok(ident, "[a-zA-Z][a-zA-Z0-9\\-_]*"),
      tok(Geq, ">="),
      tok(Leq, ">="),
  };
  static rule_def rules[] = {
      tok(module, "'MODULE' ident ';' declarations [ 'BEGIN' StatementSequence ] 'END' ident '.'"),
      tok(selector, "{ '.' ident | '[' expression ']' }"),
      tok(factor, "ident selector | number | '(' expression ')' | '~' factor"),
      tok(term, "factor { ( '*' | 'DIV' | 'MOD' | '&' ) factor } "),
      tok(SimpleExpression, "[ '+' | '-' ] term { ( '+' | '-' | 'OR' ) term }"),
      tok(expression, "SimpleExpression [ ( Leq | Geq | '=' | '#' | '>' | '<' ) SimpleExpression ]"),
      tok(assignment, "ident selector ':=' expression"),
      tok(ProcedureCall, "ident selector ActualParameters"),
      tok(statement, "[ assignment | ProcedureCall | IfStatement | WhileStatement | RepeatStatement ]"),
      tok(StatementSequence, "statement { ';' statement }"),
      tok(FieldList, "[ IdentList ':' type ]"),
      tok(type, "RecordType | ArrayType | ident"),
      tok(FPSection, "[ 'VAR' ] IdentList ':' type"),
      tok(FormalParameters, "'(' [ FPSection { ';' FPSection } ] ')'"),
      tok(ProcedureHeading, "'PROCEDURE' ident [ FormalParameters ]"),
      tok(ProcedureBody, "declarations [ 'BEGIN' StatementSequence ] 'END' ident"),
      tok(ProcedureDeclaration, "ProcedureHeading ';' ProcedureBody"),
      tok(declarations,
          "[ 'CONST' { ident '=' expression ';' } ]"
          "[ 'TYPE' { ident '=' type ';' } ]"
          "[ 'VAR' { IdentList ':' type ';' } ]"
          "{ ProcedureDeclaration ';' } "),
      tok(number, "integer"),
      tok(ActualParameters, "'(' [ expression { ',' expression } ] ')'"),
      tok(IfStatement,
          "'IF' expression 'THEN' StatementSequence"
          "{ 'ELSIF' expression 'THEN' StatementSequence }"
          "[ 'ELSE' StatementSequence ] 'END' "),
      tok(WhileStatement, "'WHILE' expression 'DO' StatementSequence 'END'"),
      tok(RepeatStatement, "'REPEAT' StatementSequence 'UNTIL' expression"),
      tok(IdentList, "ident { ',' ident }"),
      tok(ArrayType, "'ARRAY' expression 'OF' type"),
      tok(RecordType, "'RECORD' FieldList { ';' FieldList } 'END'"),
  };

  parser_t p = mk_parser(mk_rules(rules), mk_tokens(tokens), NULL);
  set_loglevel(LL_DEBUG);

  AST *a;
  static const char oberon_program[] = {
      "MODULE Samples;\n"
      "TYPE a = integer;\n"
      "END Samples.\n"};
  bool success = parse(&p, &mk_ctx(oberon_program), &a, 0);
  assert2(success);
  int expected[] = {
      -1,     // MODULE
      ident,  // Samples
      -1,     // ;
      declarations,
      -1,     // END
      ident,  // SAMPLES
      -1,     // .
  };
  AST *testast = a->first_child;
  for (int i = 0; i < LENGTH(expected); i++) {
    int ex = expected[i];
    assert2(testast->node_id == ex);
    if (ex == declarations) {
      int expected2[] = {
          -1,     // 'TYPE'
          ident,  // a
          -1,     // =
          type,   // -> ident integer
          -1,     // ;
      };
      AST *subast = testast->first_child;
      for (int i = 0; i < LENGTH(expected2); i++) {
        int ex2 = expected2[i];
        assert2(subast->node_id == ex2);
        subast = subast->next;
      }
      assert2(subast == NULL);
    }
    testast = testast->next;
  }
  assert2(testast == NULL);
  {
#define N(id, n) &(AST){.node_id = id, .next = n}
#define NC(id, fc, n) &(AST){.node_id = id, .next = n, .first_child = fc}
    AST *declarationAst = N(-1, N(ident, N(-1, NC(type, N(ident, NULL), N(-1, NULL)))));
    AST *moduleAst = N(-1, N(ident, N(-1, NC(declarations, declarationAst, N(-1, N(ident, N(-1, NULL)))))));
    AST *expectedAst = NC(module, moduleAst, NULL);
    AstCmp(a, expectedAst);
#undef N
#undef NC
  }
  destroy_ast(a);
  destroy_parser(&p);
}
static void AstCmp(AST *left, AST *right) {
  while (left || right) {
    if ((!!left) != (!!right)) {
      if (!left)
        die("Left is null, but not right");
      if (!right)
        die("Right is null, but not left");
    }
    assert2(left && right);
    assert2(left->node_id == right->node_id);
    AstCmp(left->first_child, right->first_child);
    left = left->next;
    right = right->next;
  }
}

#undef tok

int main(void) {
  setup_crash_stacktrace_logger();
  test_parser();
  test_simplest();
  test_calculator();
  test_lookahead();
  test_repeat();
  json_parser();
  test_oberon2();
  test_ll1();
  test_multiple_optionals();
  test_oberon();
  assert2(log_severity() <= LL_INFO);
  return 0;
}
