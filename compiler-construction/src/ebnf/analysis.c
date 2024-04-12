#include "ebnf/ebnf.h"

position_t get_position(const char *source, string_slice place) {
  int line, column;
  line = column = 1;
  for (; source && *source; source++) {
    if (source == place.str)
      return (position_t){.line = line, .column = column};
    if (*source == '\n') {
      line++;
      column = 1;
    } else
      column++;
  }
  return (position_t){-1, -1};
}

static void populate_terminals(terminal_list *terminals, expression_t *e) {
  v_foreach(term_t *, t, e->terms_vec) {
    v_foreach(factor_t *, f, t->factors_vec) {
      if (f->type == F_PARENS || f->type == F_OPTIONAL || f->type == F_REPEAT) {
        populate_terminals(terminals, &f->expression);
      } else if (f->type == F_STRING) {
        // TODO: compute start symbols of regex
        vec_push(&terminals->terminals_vec, (char*)f->string_regex->ctx.src);
      }
    }
  }
}

terminal_list get_terminals(const parser_t *g) {
  terminal_list t = {0};
  mk_vec(&t.terminals_vec, sizeof(char), 0);
  v_foreach(production_t *, p, g->productions_vec) {
    populate_terminals(&t, &p->expr);
  }
  return t;
}
nonterminal_list get_nonterminals(const parser_t *g) {
  nonterminal_list t = {0};
  mk_vec(&t.nonterminals_vec, sizeof(header_t), 0);
  v_foreach(production_t *, p, g->productions_vec)
      vec_push(&t.nonterminals_vec, p->header);
  return t;
}

static void populate_first_expr(const parser_t *g, struct header_t *h, expression_t *e);

static void populate_first_term(const parser_t *g, struct header_t *h, term_t *t) {
  v_foreach(factor_t *, fac, t->factors_vec) {
    switch (fac->type) {
    case F_OPTIONAL:
    case F_REPEAT:
      // these can be skipped, so the next term must be included in the first set
      populate_first_expr(g, h, &fac->expression);
      continue;
    case F_PARENS:
      populate_first_expr(g, h, &fac->expression);
      return;
    case F_IDENTIFIER: {
      // Add the first set of this production to this first set
      // TODO: if two productions recursively depend on each other,
      // this will not work. Both might get only a partial first set.
      struct header_t *id = fac->identifier.production->header;
      struct follow_t fst = {.type = FOLLOW_FIRST, .prod = id->prod};
      vec_push(&h->first_vec, &fst);
      return;
    }
    case F_STRING: {
      // TODO: compute valid start symbols of regex
      struct follow_t fst = {.type = FOLLOW_SYMBOL, .symbol = 0};
      vec_push(&h->first_vec, &fst);
      return;
    }
    }
  }
}

static void populate_first_expr(const parser_t *g, struct header_t *h, expression_t *e) {
  v_foreach(term_t *, t, e->terms_vec) {
    populate_first_term(g, h, t);
  }
}

void populate_first(const parser_t *g, struct header_t *h) {
  if (h->first) {
    return;
  }
  mk_vec(&h->first_vec, sizeof(struct follow_t), 0);
  populate_first_expr(g, h, &h->prod->expr);
}

/* follow set
 *
 */

void graph_walk(symbol_t *start, vec *all) {
  for (symbol_t *alt = start; alt; alt = alt->alt) {
    symbol_t *slow, *fast;
    slow = alt;
    fast = alt;
    while (true) {
      if (!slow)
        break;

      if (!vec_contains(all, slow)) {
        vec_push(all, slow);
        graph_walk(slow, all);
        if (slow->is_nonterminal) {
          production_t *prod = slow->nonterminal->header->prod;
          graph_walk(prod->header->sym, all);
        }
      }

      slow = slow->next;
      if (fast)
        fast = fast->next;
      if (fast)
        fast = fast->next;
      if (slow == fast)
        break;
    }
  }
}

// Walk the graph and add all symbols within k steps to the follow set
void add_symbols(symbol_t *start, int k, vec *follows) {
  if (k > 0) {
    for (symbol_t *alt = start; alt; alt = alt->alt) {
      // If the symbol is empty, we should include its continuation
      if (alt->empty) {
        add_symbols(alt->next, k, follows);
        // Otherwise, the symbol is either a literal or a production.
        // We include the continuation, and
      } else {
        struct follow_t f = {0};
        if (alt->is_nonterminal) {
          f.type = FOLLOW_FIRST;
          f.prod = alt->nonterminal;
        } else {
          f.type = FOLLOW_SYMBOL;
          // TODO: compute start symbols of regex
          f.symbol = alt->regex->ctx.src[0];
        }
        if (!vec_contains(follows, &f)) {
          vec_push(follows, &f);
          add_symbols(alt->next, k - 1, follows);
        }
      }
    }
  }
}

// Walk the graph to determine if the end of the production that a given symbol occurs in
// is reachable within k steps
bool symbol_at_end(symbol_t *start, int k) {
  if (k < 0)
    return false;
  if (start == NULL)
    return true;
  // TODO: this is not quite right.
  // If a production is encountered, we need to check the shortest number of steps through that production
  // e.g. if a production can be completed in 0 steps (e.g. it contains a single repeat)
  // if (alt->is_nonterminal) symbol_at_end(alt->next, k - min_steps(alt->nonterminal))
  for (symbol_t *alt = start; alt; alt = alt->alt) {
    // if (alt->next == NULL)
    //   return true;
    if (symbol_at_end(alt->next, alt->empty ? k : k - 1))
      return true;
  }
  return false;
}

void mega_follow_walker(const parser_t *g, symbol_t *start, vec *seen, production_t *owner) {
  const int k = 1;
  for (symbol_t *alt = start; alt; alt = alt->alt) {
    symbol_t *slow, *fast;
    slow = fast = alt;
    while (true) {
      if (!slow)
        break;

      if (!vec_contains(seen, slow)) {
        vec_push(seen, slow);
        mega_follow_walker(g, slow, seen, owner);
        // It this is a nontermninal, we should add all the symbols that
        // could follow it to its follow set.
        if (slow->is_nonterminal) {
          production_t *prod = slow->nonterminal->header->prod;
          { // apply rule 1 and 2
            if (!prod->header->follow)
              mk_vec(&prod->header->follow_vec, sizeof(struct follow_t), 0);
            // Rule 1 is applied by walking the graph k symbols forward from where
            // this production was referenced. This also applies rule 2
            // since a { repeat } expression links back with an empty transition.
            add_symbols(slow->next, k, &prod->header->follow_vec);

            // The production instance itself should also be walked.
            mega_follow_walker(g, prod->header->sym, seen, prod);
          }
          { // apply rule 3
            // Now, we need to determine if this rule is at the end of the current production
            // If this is the case, the follow set of the production that contains this nonterminal
            // must be added to the follow set as well.
            if (symbol_at_end(slow, k)) {
              struct follow_t f = {.type = FOLLOW_FOLLOW, .prod = owner};
              vec_push(&prod->header->follow_vec, &f);
            }
          }
        }
      }

      slow = slow->next;
      if (fast)
        fast = fast->next;
      if (fast)
        fast = fast->next;
      if (slow == fast) // loop detected
        break;
    }
  }
}

void populate_follow(const parser_t *g) {
  // The set of characters that can follow a given production is:
  // 1. Wherever the production occurs, the symbol that follows it is included. If a non-terminal production
  // follows, the first set of that production is included in this symbol's follow set
  // 2. If the production occurs at the end of a { repeat }, the symol at the start of the repeat is included
  // 3. If the production occurs at the end of another production, the follow set of the owning production is included
  vec seen = {0};
  mk_vec(&seen, sizeof(symbol_t), 0);
  v_foreach(production_t *, p, g->productions_vec) {
    symbol_t *start = p->header->sym;
    mega_follow_walker(g, start, &seen, p);
  }
  vec_destroy(&seen);
}

bool is_ll1(const parser_t *g);
