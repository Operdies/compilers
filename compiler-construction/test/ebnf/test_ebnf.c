// link: ebnf/ebnf.o regex.o arena.o collections.o
#include "ebnf/ebnf.h"
#include "macros.h"
#include <ctype.h>
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

void print_terminals(terminal_list tl) {
  for (int i = 0; i < tl.n_terminals; i++) {
    char ch = tl.terminals[i];
    if (isgraph(ch))
      printf("%2d) %4c\n", i, ch);
    else
      printf("%2d) 0x%02x\n", i, (int)ch);
  }
}

void print_first_sets(parser_t *g) {
  v_foreach(production_t *, p, g->productions_vec) {
    Header *h = p->header;
    populate_first(g, h);
    printf("First(%.*s) %*c ", p->identifier.n, p->identifier.str, 15 - p->identifier.n, '=');
    v_foreach(char *, chp, h->first_vec) {
      char ch = *chp;
      if (isgraph(ch))
        printf("%c ", ch);
      else
        printf("0x%02x ", (int)ch);
    }
    puts("");
  }
}

void print_tokens(tokens tok) {
  v_foreach(struct token *, t, tok.tokens_vec) {
    printf("Token '%.*s': '%.*s'\n", t->name.n, t->name.str, t->value.n, t->value.str);
  }
}

int main(void) {
  parser_t p = mk_parser(text);
  if (p.n_productions == 0) {
    fprintf(stderr, "Failed to parse grammar.\n");
    return 1;
  }
  print_first_sets(&p);
  // tokens tok = parse(&p, program);
  // print_tokens(parse(&p, program));
  // print_terminals(get_terminals(&p));
  destroy_parser(&p);
  return 0;
}
