#ifndef EBNF_H
#define EBNF_H

#include "collections.h"
#include "regex.h"
#include "text.h"
#include <stdint.h>

// * factor     = identifier | string | "(" expression ")" | "[" expression "]" | "{" expression "}".
enum factor_switch {
  F_OPTIONAL, // [ expression ]
  F_REPEAT,   // { expression }
  F_PARENS,   // ( expression )
  F_IDENTIFIER,
  F_STRING,
};

typedef struct symbol_t symbol_t;
typedef struct header_t header_t;
typedef struct factor_t factor_t;
typedef struct production_t production_t;
typedef struct term_t term_t;
typedef struct expression_t expression_t;
typedef struct identifier_t identifier_t;
typedef struct parser_t parser_t;
typedef struct position_t position_t;

struct term_t {
  string_slice range;
  vec factors_vec;
};

struct expression_t {
  string_slice range;
  vec terms_vec;
};

struct identifier_t {
  string_slice name;
  struct production_t *production;
};

struct factor_t {
  string_slice range;
  union {
    regex *regex;
    struct identifier_t identifier;
    struct expression_t expression;
  };
  enum factor_switch type;
};

struct production_t {
  // the identifier name
  string_slice identifier;
  // the parsed expression
  struct expression_t expr;
  // Header used in the parsing table
  header_t *header;
};

struct parser_t {
  parse_context ctx;
  arena *a;
  string body;
  bool backtrack;
  vec productions_vec;
};

struct symbol_t {
  regex *regex;
  symbol_t *next;
  symbol_t *alt;
  production_t *nonterminal;
  bool is_nonterminal;
  bool empty;
};

struct position_t {
  int line;
  int column;
};

struct token_t {
  string_slice name;
  string_slice value;
};

typedef struct {
  vec tokens_vec;
  bool success;
  parse_context ctx;
} tokens;

typedef struct AST AST;
struct AST {
  string_slice range;
  string_slice name;
  AST *next;
  AST *first_child;
};

parser_t mk_parser(const char *grammar);
void destroy_parser(parser_t *g);
bool parse(parser_t *g, const char *program, tokens *result);
position_t get_position(const char *source, string_slice place);

/* 5.4 exercise:
 * 1. List of terminal symbols
 * 2. List of nonterminal symbols
 * 3. The sets of start and follow symbols for each nonterminal
 *
 * Determine whether a given grammar is LL(1)
 * If not, show the conflicting productions
 */

typedef struct {
  char map[UINT8_MAX];
} terminal_list;

typedef struct {
  vec nonterminals_vec;
} nonterminal_list;

enum follow_type {
  FOLLOW_SYMBOL,
  FOLLOW_FIRST,
  FOLLOW_FOLLOW,
};

struct follow_t {
  enum follow_type type;
  regex *regex;
  production_t *prod;
};

struct header_t {
  production_t *prod;
  symbol_t *sym;
  vec first_vec;
  vec follow_vec;
};

terminal_list get_terminals(const parser_t *g);
nonterminal_list get_nonterminals(const parser_t *g);
void populate_first(struct header_t *h);
void populate_follow(const parser_t *g);
bool is_ll1(const parser_t *g);
void graph_walk(symbol_t *start, vec *all);
vec populate_maps(production_t *owner, vec follows);
void add_symbols(symbol_t *start, int k, vec *follows);

#endif // !EBNF_H
