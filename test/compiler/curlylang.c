// link ebnf/ebnf.o logging.o collections.o scanner/scanner.o regex.o arena.o
#include <assert.h>

#include "ebnf/ebnf.h"
#include "logging.h"
#include "macros.h"
enum tokens {
  statement_list,
  statement,
  expression,
  term,
  factor,
  number,
  identifier,
};

static char test_program[] = {
#include "test.curls"
};

#define tok(key, pattern) [key] = {#key, (char *)pattern}
int main(void) {
  set_loglevel(LL_DEBUG);
  rule_def rules[] = {
      tok(statement_list, "statement { statement }"),
      tok(statement, "expression ';' | '{' statement_list '}'"),
      tok(expression, "term {('+' | '-') term }"),
      tok(term, "factor {('*' | '/') factor }"),
      tok(factor, "'(' expression ')' | number | identifier "),
  };
  token_def tokens[] = {
      tok(number, "[0-9]+"),
      tok(identifier, "[a-zA-Z_][a-zA-Z_0-9]*"),
  };
  parser_t p = mk_parser(mk_rules(rules), mk_tokens(tokens), "(/\\*.*\\*/)|//[^\n]*");
  AST *a = NULL;
  bool success = parse(&p, &mk_ctx(test_program), &a, 0);
  if (!success) {
    debug_ctx(p.s->ctx);
    die("Parse failed.");
  }
  destroy_ast(a);
  destroy_parser(&p);
  return 0;
}
