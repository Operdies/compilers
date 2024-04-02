// link: ebnf/ebnf.o regex.o arena.o collections.o
#include "ebnf/ebnf.h"
#include "macros.h"
#include <stdio.h>

static const char text[] = {
    // "expr   = term   | expr '+' term | expr '-' term .\n"
    // "term   = factor | term '/' factor | term '*' factor.\n"
    // "factor = digit  | '(' expr ')'.\n"
    // "digits = digit { digit }.\n"
    "expression = term {(plus | minus ) term } { 'yo' } .\n"
    "term       = factor {(mult | div) factor } .\n"
    "factor     = { whitespace } ( digits | '(' expression ')' ) [ '!' ] { whitespace }  .\n"
    "plus       = '+'.\n"
    "minus      = '-' .\n"
    "mult       = '*'.\n"
    "div        = '/'.\n"
    "exlm       = '!'.\n"
    "digits     = digit { digit } .\n"
    "whitespace = ' ' | '\n' | '\t' .\n"
    "digit      = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' .\n"};
// "digit  = [0-9].\n"};

static const char program[] = {" 12-34   +   (3  *( 4+2  )-1)/1-23"};

int main(void) {
  parser_t p = mk_parser(text);
  if (p.n_productions == 0) {
    fprintf(stderr, "Failed to parse grammar.\n");
    return 1;
  }
  tokens tok = parse(&p, program);
  // v_foreach(struct token *, t, tok.tokens_vec) {
  //   printf("Token '%.*s': '%.*s'\n", t->name.n, t->name.str, t->value.n, t->value.str);
  // }
  destroy_parser(&p);
  return 0;
}
