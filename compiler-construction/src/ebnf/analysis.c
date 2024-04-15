#include "ebnf/ebnf.h"
#include "logging.h"
#include "macros.h"
#include "regex.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

void populate_terminals(terminal_list *terminals, expression_t *e) {
  v_foreach(term_t *, t, e->terms_vec) {
    v_foreach(factor_t *, f, t->factors_vec) {
      if (f->type == F_PARENS || f->type == F_OPTIONAL || f->type == F_REPEAT) {
        populate_terminals(terminals, &f->expression);
      } else if (f->type == F_STRING) {
        regex_first(f->regex, terminals->map);
      }
    }
  }
}

terminal_list get_terminals(const parser_t *g) {
  terminal_list t = {0};
  v_foreach(production_t *, p, g->productions_vec) {
    populate_terminals(&t, &p->expr);
  }
  return t;
}
nonterminal_list get_nonterminals(const parser_t *g) {
  nonterminal_list t = {.nonterminals_vec = v_make(header_t)};
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
      struct follow_t fst = {.type = FOLLOW_SYMBOL, .regex = fac->regex};
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
  h->first_vec = v_make(struct follow_t);
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
          f.regex = alt->regex;
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
              prod->header->follow_vec = v_make(struct follow_t);
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
  vec seen = v_make(symbol_t);
  v_foreach(production_t *, p, g->productions_vec) {
    symbol_t *start = p->header->sym;
    mega_follow_walker(g, start, &seen, p);
  }
  vec_destroy(&seen);
}

typedef char MAP[UINT8_MAX];
void expand_first(struct follow_t *follow, char reachable[static UINT8_MAX], vec *seen) {
  if (vec_contains(seen, follow))
    return;
  vec_push(seen, follow);

  switch (follow->type) {
  case FOLLOW_SYMBOL:
    regex_first(follow->regex, reachable);
    break;
  case FOLLOW_FIRST: {
    v_foreach(struct follow_t *, fst, follow->prod->header->first_vec)
        expand_first(fst, reachable, seen);
  } break;
  case FOLLOW_FOLLOW: {
    v_foreach(struct follow_t *, fol, follow->prod->header->follow_vec)
        expand_first(fol, reachable, seen);
  } break;
  }
}

typedef struct {
  MAP set;
  production_t *prod;
} record;

typedef struct {
  production_t *A;
  production_t *B;
  char ch;
  bool first;
  production_t *owner;
} conflict;

vec populate_maps(production_t *owner, int n, struct follow_t follows[static n]) {
  vec map = v_make(record);
  for (int i = 0; i < n; i++) {
    struct follow_t *follow = &follows[i];
    // in the first set of everything that can follow this production,
    // are there any intersections?
    record r = {.prod = owner};

    switch (follow->type) {
    case FOLLOW_SYMBOL:
      regex_first(follow->regex, r.set);
      break;
    case FOLLOW_FOLLOW:
    case FOLLOW_FIRST: {
      vec seen = v_make(struct follow_t);
      r.prod = follow->prod;
      expand_first(follow, r.set, &seen);
      vec_destroy(&seen);
      break;
    }
    }
    vec_push(&map, &r);
  }
  return map;
}

bool check_intersection(int n, record records[static n], conflict *c) {
  for (int i = 0; i < UINT8_MAX; i++) {
    production_t *seen = NULL;
    for (int j = 0; j < n; j++) {
      record *r = &records[j];
      if (r->set[i]) {
        if (seen) {
          *c = (conflict){.A = seen, .B = r->prod, .ch = i};
          return true;
        }
        seen = r->prod;
      }
    }
  }
  return false;
}

bool get_conflicts(const header_t *h, conflict *c) {
  *c = (conflict){0};
  {
    vec first_map = populate_maps(h->prod, h->n_first, h->first);
    bool intersect = check_intersection(first_map.n, first_map.array, c);
    vec_destroy(&first_map);
    c->first = true;
    c->owner = h->prod;
    if (intersect)
      return true;
  }
  {
    vec follow_map = populate_maps(h->prod, h->n_follow, h->follow);
    bool intersect = check_intersection(follow_map.n, follow_map.array, c);
    vec_destroy(&follow_map);
    c->first = false;
    if (intersect)
      return true;
  }

  return false;
}

bool is_ll1(const parser_t *g) {
  bool isll1 = true;

  v_foreach(production_t *, p, g->productions_vec)
      populate_first(g, p->header);
  populate_follow(g);
  nonterminal_list nt = get_nonterminals(g);

  // TEST FIRST CONFLICTS
  conflict c = {0};
  v_foreach(header_t *, h, nt.nonterminals_vec) {
    if (get_conflicts(h, &c)) {
      string_slice id1 = c.A->identifier;
      string_slice id2 = c.B->identifier;
      string_slice id3 = c.owner->identifier;
      debug("Productions '%.*s' and '%.*s' are in conflict.\nBoth allow char '%c'\nIn '%s' set of '%.*s'", id1.n, id1.str, id2.n, id2.str, c.ch, c.first ? "first" : "follow", id3.n, id3.str);
      isll1 = false;
    }
  }

  vec_destroy(&nt.nonterminals_vec);

  return isll1;
}
