#include "interpreters/lisp/test_lisp_compiler.h"

#include <stdlib.h>

#include "ebnf/ebnf.h"
#include "logging.h"
#include "macros.h"
#include "scanner/scanner.h"
#include "text.h"

static parser_t *parser = NULL;
/** Goal:
 * Simple lisp dialect. The goal is to write a small interpreter,
 * not to create a useful language. I barely even know how lisp works,
 */
static void mk_lisp_parser(void) {
#define tok(key, pattern) [key] = {#key, (char *)pattern}
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
    string,
  };

  const token_def tokens[] = {
      tok(letter, "[a-zA-Z]"), tok(identifier, "[a-zA-Z][a-zA-Z_\\.:]*"), tok(digit, "[0-9]"),
      tok(number, "[0-9]+"),   tok(symbol, "'[a-zA-Z0-9_\\-]+"),          tok(string, "\"([^\"\\\\]|\\\\.)*\""),
  };

  const rule_def rules[] = {
      tok(slist, "sexpr { sexpr }"),
      tok(sexpr, "identifier | '+' | '-' | number | symbol | string | '(' slist ')'"),
      tok(function, "identifier | '+' | '-'"),
  };
#undef tok

  parser_t p = mk_parser(mk_rules(rules), mk_tokens(tokens), ";[^\n]*");
  parser = ecalloc(1, sizeof(parser_t));
  *parser = p;
}

struct lispObject lisp_eval(const char *expression) {
  if (parser == NULL) {
    mk_lisp_parser();
    atexit_r(destroy_parser, parser);
    atexit_r(free, parser);
  }

  struct lispObject o = {0};
  return o;
  AST *program;
  parse_context ctx = mk_ctx(expression);
  bool success = parse(parser, &ctx, &program, 0);
  if (success) {
    // print_ast(program);
    destroy_ast(program);
  }
  if (!success) {
    info("Parse failed at cursor %d of %d", ctx.c, ctx.view.n);
    info_ctx(&ctx);
    die("");
  }
}
