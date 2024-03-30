// link: ebnf.o regex.o arena.o collections.o
#include "ebnf.h"
#include "macros.h"
#include <stdio.h>

static const char text[] = {
    "expr   = term   | expr '+' term | expr '-' term .\n"
    "term   = factor | term '/' factor | term '*' factor.\n"
    "factor = digit  | '(' expr ')'.\n"
    "digits = digit { digit }.\n"
    "digit  = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' .\n"};
// "digit  = [0-9].\n"};

static const char program[] = {"1+2*3"};

int main(void) {
  printf("Grammar:\n%s\n", text);
  grammar_t g = parse_grammar(text);
  if (g.n == 0) {
    fprintf(stderr, "Failed to parse grammar.\n");
    return 1;
  }
  term_t t = g.productions[4].expr.terms[2];
  position_t p = get_position(text, t.range);
  printf("Term %.*s at position %d:%d\n", t.range.n, t.range.str, p.line, p.column);
  parser_t parser = tokenize(&g, program);
  (void)parser;
  destroy_grammar(&g);
  return 0;
}
