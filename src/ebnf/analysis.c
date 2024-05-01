#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "collections.h"
#include "ebnf/ebnf.h"
#include "logging.h"
#include "macros.h"
#include "regex.h"

typedef char MAP[UINT8_MAX];
typedef struct {
  MAP set;
  const production_t *prod;
} record;

typedef struct {
  const production_t *A;
  const production_t *B;
  char ch;
  bool first;
  const production_t *owner;
} conflict;

static bool populate_first_expr(production_t *h, expression_t *e);
static bool expression_optional(expression_t *expr);
static bool factor_optional(factor_t *fac);
static bool expression_optional(expression_t *expr);
static bool populate_first_term(production_t *h, term_t *t);
static bool populate_first_expr(production_t *h, expression_t *e);
static bool symbol_at_end(symbol_t *start, int k);
static void mega_follow_walker(const parser_t *g, symbol_t *start, vec *seen, production_t *owner);
static void expand_first(struct follow_t *follow, char reachable[static UINT8_MAX], vec *seen);
static bool check_intersection(int n, record records[static n], conflict *c);
static vec first_expr_helper(expression_t *expr);
static bool get_conflicts(const production_t *h, conflict *c);

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
  v_foreach(term_t, t, e->terms_vec) {
    v_foreach(factor_t, f, t->factors_vec) {
      if (f->type == F_PARENS || f->type == F_OPTIONAL || f->type == F_REPEAT) {
        populate_terminals(terminals, &f->expression);
      } else if (f->type == F_STRING) {
        int fst = f->string.str[0];
        terminals->map[fst] = 1;
      }
    }
  }
}

terminal_list get_terminals(const parser_t *g) {
  terminal_list t = {0};
  v_foreach(production_t, p, g->productions_vec) { populate_terminals(&t, &p->expr); }
  return t;
}

nonterminal_list get_nonterminals(const parser_t *g) {
  nonterminal_list t = {.nonterminals_vec = v_make(production_t)};
  v_foreach(production_t, p, g->productions_vec) vec_push(&t.nonterminals_vec, p);
  return t;
}

bool factor_optional(factor_t *fac) {
  switch (fac->type) {
    case F_OPTIONAL:
    case F_REPEAT:
      return true;
    case F_PARENS:
      return expression_optional(&fac->expression);
    case F_IDENTIFIER: {
      return expression_optional(&fac->identifier.production->expr);
    }
    case F_TOKEN: {
      regex *r = fac->token->pattern;
      regex_match m = regex_matches(r, &mk_ctx(""));
      return m.match;
    }
    case F_STRING: {
      return false;
    }
  }
  return false;
}

bool expression_optional(expression_t *expr) {
  v_foreach(term_t, t, expr->terms_vec) {
    v_foreach(factor_t, f, t->factors_vec) {
      if (!factor_optional(f))
        return false;
    }
  }
  return true;
}

bool populate_first_term(production_t *h, term_t *t) {
  v_foreach(factor_t, fac, t->factors_vec) {
    switch (fac->type) {
      case F_OPTIONAL:
      case F_REPEAT:
        // these can be skipped, so the next term must be included in the first set
        populate_first_expr(h, &fac->expression);
        continue;
      case F_PARENS:
        if (populate_first_expr(h, &fac->expression) || expression_optional(&fac->expression))
          continue;
        return false;
      case F_IDENTIFIER: {
        production_t *id = fac->identifier.production;
        struct follow_t fst = {.type = FOLLOW_FIRST, .prod = id};
        vec_push(&h->first_vec, &fst);
        if (expression_optional(&id->expr))
          continue;
        return false;
      }
      case F_STRING: {
        struct follow_t fst = {.type = FOLLOW_CHAR, .ch = fac->string.str[0]};
        vec_push(&h->first_vec, &fst);
        return false;
        break;
      }
      case F_TOKEN: {
        regex *r = fac->token->pattern;
        struct follow_t fst = {.type = FOLLOW_SYMBOL, .regex = r};
        vec_push(&h->first_vec, &fst);
        regex_match m = regex_matches(r, &(match_context){.n = 0, .src = ""});
        if (m.match)
          continue;
        return false;
      }
    }
  }
  return true;
}

bool populate_first_expr(production_t *h, expression_t *e) {
  bool all_optional = true;
  v_foreach(term_t, t, e->terms_vec) {
    if (!populate_first_term(h, t))
      all_optional = false;
  }
  return all_optional;
}

void populate_first(production_t *h) {
  if (h->first_vec.n) {
    return;
  }
  h->first_vec = v_make(struct follow_t);
  if (populate_first_expr(h, &h->expr)) {
    // debug("%.*s is completely optional", h->prod->identifier.n, h->prod->identifier.str);
  }
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
        if (slow->type == nonterminal_symbol) {
          production_t *prod = slow->nonterminal;
          graph_walk(prod->sym, all);
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
      struct follow_t f = {0};
      switch (alt->type) {
        case error_symbol:
          die("Error symbol??");
        case empty_symbol:
          add_symbols(alt->next, k, follows);
          continue;
          break;
        case nonterminal_symbol:
          f.type = FOLLOW_FIRST;
          f.prod = alt->nonterminal;
          break;
        case token_symbol:
          f.type = FOLLOW_SYMBOL;
          f.regex = alt->token->pattern;
          break;
        case string_symbol:
          f.type = FOLLOW_CHAR;
          f.ch = alt->string.str[0];
          break;
      }
      if (!vec_contains(follows, &f)) {
        vec_push(follows, &f);
        add_symbols(alt->next, k - 1, follows);
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
  for (symbol_t *alt = start; alt; alt = alt->alt) {
    // TODO: this doesn't generalize to k > 1
    if (alt->type == nonterminal_symbol && expression_optional(&alt->nonterminal->expr)) {
      return symbol_at_end(alt->next, k);
    }
    // if (alt->next == NULL)
    //   return true;
    if (symbol_at_end(alt->next, alt->type == empty_symbol ? k : k - 1))
      return true;
  }
  return false;
}

void mega_follow_walker(const parser_t *g, symbol_t *start, vec *seen, production_t *owner) {
  const int lookahead = 1;
  // NOTE: we assume that alt loops are not possible.
  // They should be impossible by construction.
  // If an alt loop occurs, it is a bug.
  for (symbol_t *alt = start; alt; alt = alt->alt) {
    // hare and tortoise solution to detect loops, which are very possible for next chains
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
        if (slow->type == nonterminal_symbol) {
          production_t *prod = slow->nonterminal;
          {  // apply rule 1 and 2
            if (!prod->follow_vec.n)
              prod->follow_vec = v_make(struct follow_t);
            for (symbol_t *this = slow->next; this; this = this->alt) {
              add_symbols(this, lookahead, &prod->follow_vec);
            }

            // The production instance itself should also be walked.
            mega_follow_walker(g, prod->sym, seen, prod);
          }
          {  // apply rule 3
            // Now, we need to determine if this rule is at the end of the current production
            // If this is the case, the follow set of the production that contains this nonterminal
            // must be added to the follow set as well.
            if (symbol_at_end(slow, lookahead)) {
              struct follow_t f = {.type = FOLLOW_FOLLOW, .prod = owner};
              vec_push(&prod->follow_vec, &f);
            }
          }
        }
      }

      slow = slow->next;
      if (fast)
        fast = fast->next;
      if (fast)
        fast = fast->next;
      if (slow == fast)  // loop detected
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
  v_foreach(production_t, p, g->productions_vec) {
    symbol_t *start = p->sym;
    mega_follow_walker(g, start, &seen, p);
  }
  vec_destroy(&seen);
}

// populate a map of all the symbols that can be reached
void expand_first(struct follow_t *follow, char reachable[static UINT8_MAX], vec *seen) {
  if (vec_contains(seen, follow))
    return;
  vec_push(seen, follow);

  switch (follow->type) {
    case FOLLOW_SYMBOL:
      regex_first(follow->regex, reachable);
      break;
    case FOLLOW_FIRST: {
      v_foreach(struct follow_t, fst, follow->prod->first_vec) { expand_first(fst, reachable, seen); }
    } break;
    case FOLLOW_FOLLOW: {
    } break;
    case FOLLOW_CHAR: {
      reachable[(int)follow->ch] = 1;
    } break;
  }
}

vec populate_maps(const production_t *owner, vec follows) {
  vec map = v_make(record);
  v_foreach(struct follow_t, follow, follows) {
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
      case FOLLOW_CHAR:
        r.set[(int)follow->ch] = 1;
        break;
    }
    vec_push(&map, &r);
  }

  return map;
}

bool check_intersection(int n, record records[static n], conflict *c) {
  for (int i = 0; i < UINT8_MAX; i++) {
    const production_t *seen = NULL;
    for (int j = 0; j < n; j++) {
      record *r = &records[j];
      if (r->set[i]) {
        if (seen) {
          c->A = seen;
          c->B = r->prod;
          c->ch = i;
          return true;
        }
        seen = r->prod;
      }
    }
  }
  return false;
}

vec first_expr_helper(expression_t *expr) {
  production_t temp_header = {.first_vec = v_make(struct follow_t)};
  populate_first_expr(&temp_header, expr);
  return temp_header.first_vec;
}

bool get_conflicts(const production_t *h, conflict *c) {
  *c = (conflict){0};
  c->owner = h;
  {
    // 1. term0 | term1    -> the terms must not have any common start symbols
    // 2. fac0 fac1        -> if fac0 contains the empty sequence, then the factors must not have any common start
    // symbols
    vec first_map = populate_maps(h, h->first_vec);
    bool intersect = check_intersection(first_map.n, first_map.array, c);
    vec_destroy(&first_map);
    if (intersect) {
      c->first = true;
      return true;
    }
  }

  vec follow_map = populate_maps(h, h->follow_vec);
  bool conflict = false;

  {
    // 3 [exp] or {exp}    -> the sets of start symbols of exp and of symbols that may follow K must be disjoint
    v_foreach(term_t, term, h->expr.terms_vec) {
      v_rforeach(factor_t, fac, term->factors_vec) {
        bool optional = false;
        if (fac->type == F_OPTIONAL || fac->type == F_REPEAT ||
            (fac->type == F_PARENS && expression_optional(&fac->expression))) {
          optional = true;
          vec expr_first = first_expr_helper(&fac->expression);
          vec map1 = populate_maps(h, expr_first);
          vec_destroy(&expr_first);
          vec_push_slice(&map1, &follow_map.slice);
          conflict = check_intersection(map1.n, map1.array, c);
          vec_destroy(&map1);
          if (conflict) {
            goto done;
          }
        } else if (fac->type == F_IDENTIFIER) {
          if (expression_optional(&fac->identifier.production->expr)) {
            optional = true;
            production_t *p = fac->identifier.production;
            vec map1 = populate_maps(p, p->first_vec);
            vec_push_slice(&map1, &follow_map.slice);
            conflict = check_intersection(map1.n, map1.array, c);
            vec_destroy(&map1);
            if (conflict)
              goto done;
          }
        } else if (fac->type == F_STRING) {  // F_STRING
          optional = false;
        } else if (fac->type == F_TOKEN) {  // F_STRING
          if (regex_matches(fac->token->pattern, &(match_context){.src = ""}).match) {
            optional = true;
            struct follow_t tmpf = {.prod = h, .regex = fac->token->pattern, .type = FOLLOW_SYMBOL};
            vec map1 = populate_maps(h, (vec){.sz = sizeof(tmpf), .n = 1, .array = &tmpf});
            vec_push_slice(&map1, &follow_map.slice);
            conflict = check_intersection(map1.n, map1.array, c);
            vec_destroy(&map1);
            if (conflict)
              goto done;
          }
        }
        if (!optional)  // done
          break;
      }
    }
  }
done:
  vec_destroy(&follow_map);
  // {
  //   vec follow_map = populate_maps(h->prod, h->n_follow, h->follow);
  //   bool intersect = check_intersection(follow_map.n, follow_map.array, c);
  //   vec_destroy(&follow_map);
  //   c->first = false;
  //   if (intersect)
  //     return true;
  // }

  return conflict;
}

bool is_ll1(const parser_t *g) {
  bool isll1 = true;

  v_foreach(production_t, p, g->productions_vec) populate_first(p);
  populate_follow(g);
  nonterminal_list nt = get_nonterminals(g);

  // TEST FIRST CONFLICTS
  conflict c = {0};
  v_foreach(production_t, h, nt.nonterminals_vec) {
    if (get_conflicts(h, &c)) {
      string_slice id1 = c.A->identifier;
      string_slice id2 = c.B->identifier;
      string_slice id3 = c.owner->identifier;
      debug(
          "Productions '%.*s' and '%.*s' are in conflict.\n"
          "Both allow char '%c'\n"
          "In '%s' set of '%.*s'",
          id1.n, id1.str, id2.n, id2.str, c.ch, c.first ? "first" : "follow", id3.n, id3.str);
      isll1 = false;
    }
  }

  vec_destroy(&nt.nonterminals_vec);

  return isll1;
}

/*
 * Utilities for printing information about a grammar

void print_sym(symbol_t *sym) {
  const char *s = describe_symbol(sym);
  info(s);
}

static void print_follow_set(vec *v, vec *seen, char map[UINT8_MAX]);
static void print_tokens(tokens tok);
static void print_map(char map[UINT8_MAX]);
static void print_nonterminals(nonterminal_list ntl);
static void print_terminals(terminal_list tl);
static void print_first_sets(parser_t *g);
static void print_follow_sets(parser_t *g);
static void print_enumerated_graph(vec all);
static void print_follow_set(vec *v, vec *seen, char map[UINT8_MAX]);

void print_map(char map[UINT8_MAX]) {
  for (int sym = 0; sym < UINT8_MAX; sym++) {
    if (map[sym]) {
      if (isgraph(sym))
        printf("%c ", sym);
      else
        printf("0x%x ", sym);
    }
  }
}

void print_tokens(tokens tok) {
  v_foreach(struct token_t *, t, tok.tokens_vec) {
    info("%3d Token '%.*s'\n%.*s'\n", idx_t, t->name.n, t->name.str, t->value.n, t->value.str);
  }
}
void print_nonterminals(nonterminal_list ntl) {
  vec buf = v_make(char);
  v_foreach(production_t *, h, ntl.nonterminals_vec) {
    production_t *p = h->prod;
    vec_write(&buf, "%.*s, ", p->identifier.n, p->identifier.str);
  }
  info("Nonterminals: %.*s", buf.n, buf.array);
  vec_destroy(&buf);
}
void print_terminals(terminal_list tl) {
  vec buf = v_make(char);
  for (int i = 0; i < LENGTH(tl.map); i++) {
    if (tl.map[i]) {
      char ch = (char)i;
      if (isgraph(ch))
        vec_write(&buf, "%c, ", ch);
      else
        vec_write(&buf, "%02x, ", (int)ch);
    }
  }
  info("Terminals: %.*s", buf.n, buf.array);
  vec_destroy(&buf);
}
void print_first_sets(parser_t *g) {
  {
    v_foreach(production_t *, p, g->productions_vec) {
      production_t *h = p;
      populate_first(h);
    }
  }
  v_foreach(production_t *, p, g->productions_vec) {
    production_t *h = p;
    char *ident = string_slice_clone(p->identifier);
    printf(" First(%22s) %2c  ", ident, '=');
    vec seen = v_make(vec);
    char map[UINT8_MAX] = {0};
    print_follow_set(&h->first_vec, &seen, map);
    vec_destroy(&seen);
    print_map(map);
    puts("");
    free(ident);
  }
}
void print_follow_sets(parser_t *g) {
  populate_follow(g);
  v_foreach(production_t *, p, g->productions_vec) {
    production_t *h = p;
    // vec follow = populate_maps(p, h->n_follow, h->follow);
    char *ident = string_slice_clone(p->identifier);
    printf("Follow(%22s) %2c  ", ident, '=');
    vec seen = v_make(vec);
    char map[UINT8_MAX] = {0};
    print_follow_set(&h->follow_vec, &seen, map);
    vec_destroy(&seen);
    print_map(map);
    puts("");
    free(ident);
    // vec_destroy(&follow);
  }
}
void print_enumerated_graph(vec all) {
  v_foreach(symbol_t *, sym, all) {
    int idx = idx_sym;
    printf("%2d) ", idx);
    print_sym(sym);
  }
}

void print_follow_set(vec *v, vec *seen, char map[UINT8_MAX]) {
  if (vec_contains(seen, v))
    return;
  vec_push(seen, v);
  v_foreach(struct follow_t *, sym, (*v)) {
    switch (sym->type) {
    case FOLLOW_SYMBOL: {
      regex_first(sym->regex, map);
      break;
    }
    case FOLLOW_FIRST:
      // printf("First(%.*s) ", sym->prod->identifier.n, sym->prod->identifier.str);
      print_follow_set(&sym->prod->first_vec, seen, map);
      break;
    case FOLLOW_FOLLOW:
      // printf("Follow(%.*s) ", sym->prod->identifier.n, sym->prod->identifier.str);
      print_follow_set(&sym->prod->follow_vec, seen, map);
      break;
    case FOLLOW_CHAR:
      map[(int)sym->ch] = 1;
      break;
    }
  }
}
 */
