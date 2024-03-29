// link: ebnf.o regex.o arena.o
#include "ebnf.h"
#include "macros.h"
#include <stdio.h>

static const char *text = {
    "expr   = term   | expr '+' term.\n"
    "term   = factor | term '*' factor.\n"
    "factor = digit  | '(' expr ')'.\n"
    "digit  = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' .\n"};
// "digit  = [0-9].\n"};

int main(void) {
  printf("Grammar:\n%s\n", text);
  grammar *g = parse_grammar(text);
  if (g == NULL) {
    fprintf(stderr, "Failed to parse grammar.\n");
    return 1;
  }
  // for (size_t i = 0; i < g->n_rules; i++) {
  //   production *r = &g->productions[i];
  //   printf("%2ld) %.*s\n", i, r->src.len, r->src.str);
  // }
  return 0;
}
