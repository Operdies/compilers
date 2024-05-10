// link ebnf/ebnf.o ebnf/analysis.o scanner/scanner.o
// link regex.o arena.o collections.o logging.o
#include "../unittest.h"
#include "ebnf/ebnf.h"
#include "logging.h"
#include "macros.h"
#include "scanner/scanner.h"
#include "text.h"

#define tok(key, pattern) [key] = {#key, (char *)pattern}

static char test_program[] = {
#include "test.lisp"
};

int main(void) {
  set_loglevel(LL_DEBUG);
  setup_crash_stacktrace_logger();
  enum tokens {
    slist,
    sexpr,
    lpar,
    rpar,
    quot,
    identifier,
    function,
    symbol,
    letter,
    digit,
    number,
    dot,
    comment,
    string,
  };

  const token_def tokens[] = {
      tok(letter, "[a-zA-Z]"), tok(identifier, "[a-zA-Z][a-zA-Z_\\.:]*"), tok(digit, "[0-9]"),
      tok(number, "[0-9]+"),   tok(symbol, "'[a-zA-Z0-9_\\-]+"),          tok(string, "\"([^\"\\\\]|\\\\.)*\""),
      tok(comment, ";[^\n]*"),
  };

  const rule_def rules[] = {
      tok(slist, "sexpr { sexpr }"),
      tok(sexpr, "comment | identifier | '+' | '-' | number | symbol | string | '(' slist ')'"),
      tok(function, "identifier | '+' | '-'"),
  };

  /** Goal:
   * Simple lisp dialect.
   * 1. No separate namespace for functions and variables
   * 2. Higher order functions
   */

  parser_t p = mk_parser(mk_rules(rules), mk_tokens(tokens));

  AST *program;
  parse_context ctx = mk_ctx(test_program);
  bool success = parse(&p, &ctx, &program, slist);
  if (success) {
    // print_ast(program);
    destroy_ast(program);
  }
  if (!success) {
    info("Parse failed at cursor %d of %d", ctx.c, ctx.n);
    info_ctx(&ctx);
  }
  assert2(success);
  assert2(log_severity() <= LL_INFO);
  return 0;
}
