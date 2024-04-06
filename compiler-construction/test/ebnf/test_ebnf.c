// link ebnf/ebnf.o ebnf/analysis.o
// link regex.o arena.o collections.o
#include "ebnf/ebnf.h"
#include "macros.h"
#include <ctype.h>
#include <stdio.h>

static const char text[] = {
    // "expr   = term   | expr '+' term | expr '-' term .\n"
    // "term   = factor | term '/' factor | term '*' factor.\n"
    // "factor = digit  | '(' expr ')'.\n"
    // "digits = digit { digit }.\n"
    "expression = term {('+' | '-' ) term } .\n"
    "term       = factor {('*' | '/') factor } .\n"
    "factor     = ( digits | '(' expression ')' ) .\n"
    "digits     = digit { digit } .\n"
    "digit      = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' .\n"};
// "digit  = [0-9].\n"};

/* Follow:
 * expression: ')'
 * term: '+' '-' follow(expression)
 * factor: '*' '/' follow(term)
 * Digits: follow(factor)
 * digit: first(digit) follow(digits)
 */
static const char program[] = {"12-34+(3*(4+2)-1)/1-23"};
void print_terminals(terminal_list tl) {
  for (int i = 0; i < tl.n_terminals; i++) {
    char ch = tl.terminals[i];
    if (isgraph(ch))
      printf("%2d) %4c\n", i, ch);
    else
      printf("%2d) 0x%02x\n", i, (int)ch);
  }
}

static void print_sym(symbol_t *sym) {
  if (sym->empty)
    printf("empty\n");
  else if (sym->is_nonterminal)
    printf("production '%.*s'\n", sym->nonterminal->identifier.n, sym->nonterminal->identifier.str);
  else if (isgraph(sym->sym))
    printf("symbol '%c'\n", sym->sym);
  else
    printf("symbol 0x%02x\n", (int)sym->sym);
}

void print_first_sets(parser_t *g) {
  v_foreach(production_t *, p, g->productions_vec) {
    header_t *h = p->header;
    populate_first(g, h);
    printf("First(%.*s)  %*c ", p->identifier.n, p->identifier.str, 15 - p->identifier.n, '=');
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

void print_follow_sets(parser_t *g) {
  puts("\n\n\n");
  populate_follow(g);
  v_foreach(production_t *, p, g->productions_vec) {
    header_t *h = p->header;
    printf("Follow(%.*s) %*c\n", p->identifier.n, p->identifier.str, 15 - p->identifier.n, '=');
    v_foreach(struct follow_t *, sym, h->follow_vec) {
      switch (sym->type) {
      case FOLLOW_SYMBOL:
        if (isgraph(sym->symbol))
          printf("Symbol('%c')\n", sym->symbol);
        else
          printf("Symbol(0x%x)\n", sym->symbol);
        break;
      case FOLLOW_FIRST:
        printf("First(%.*s)\n", sym->prod->identifier.n, sym->prod->identifier.str);
        break;
      case FOLLOW_FOLLOW:
        printf("Follow(%.*s)\n", sym->prod->identifier.n, sym->prod->identifier.str);
        break;
      }
    }
    puts("");
  }
}

void print_tokens(tokens tok) {
  v_foreach(struct token_t *, t, tok.tokens_vec) {
    printf("Token '%.*s': '%.*s'\n", t->name.n, t->name.str, t->value.n, t->value.str);
  }
}

void print_enumerated_graph(vec all) {
  v_foreach(symbol_t *, sym, all) {
    int idx = idx_sym;
    printf("%2d) ", idx);
    print_sym(sym);
  }
}

int main(void) {
  parser_t p = mk_parser(text);
  if (p.n_productions == 0) {
    fprintf(stderr, "Failed to parse grammar.\n");
    return 1;
  }
  vec all = {0};
  mk_vec(&all, sizeof(symbol_t), 0);
  graph_walk(p.productions[0].header->sym, &all);
  // v_foreach(production_t *, prod, p.productions_vec) {
  //   graph_walk(prod->header->sym, &all);
  // }
  // print_enumerated_graph(all);
  // print_first_sets(&p);
  print_follow_sets(&p);
  // populate_follow(&p);
  // print_tokens(parse(&p, program));
  // print_terminals(get_terminals(&p));
  destroy_parser(&p);
  return 0;
}
