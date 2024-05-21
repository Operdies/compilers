#ifndef EBNF_H
#define EBNF_H

#include <stdint.h>

#include "arena.h"
#include "collections.h"
#include "regex.h"
#include "scanner/scanner.h"
#include "text.h"

// * factor     = identifier | string | "(" expression ")" | "[" expression "]" | "{" expression "}".
enum factor_switch {
  F_OPTIONAL,  // [ expression ]
  F_REPEAT,    // { expression }
  F_PARENS,    // ( expression )
  F_IDENTIFIER,
  F_STRING,
  F_TOKEN,
};

typedef struct symbol_t symbol_t;
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
    string_slice string;
    struct identifier_t identifier;
    struct expression_t expression;
    token *token;
  };
  enum factor_switch type;
};

struct production_t {
  string_slice identifier;   // the identifier name
  struct expression_t expr;  // the parsed expression
  int id;                    // the index of this production in the input to the parser
  symbol_t *sym;             // the start symbol of this production
  vec first_vec;             // the set of follow_t that can occur in the beginning of this production
  vec follow_vec;            // the set of symbols that can follow this production
};

struct parser_t {
  parse_context ctx;
  arena *a;
  union {
    vec productions_vec;
    struct {
      int n;
      production_t *productions;
    };
  };
  scanner *s;
};

enum symbol_type {
  error_symbol,
  empty_symbol,
  nonterminal_symbol,
  token_symbol,
  string_symbol,
};

struct symbol_t {
  string_slice string;
  symbol_t *next;
  symbol_t *alt;
  production_t *nonterminal;
  token *token;
  enum symbol_type type;
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
  int node_id;
  AST *parent;
  AST *next;
  AST *first_child;
};

typedef struct {
  const char *id;
  const char *rule;
} rule_def;

typedef struct {
  int n;
  const rule_def *rules;
} grammar_rules;

#define mk_rules(r) \
  (grammar_rules) { .n = LENGTH(r), .rules = r }

parser_t mk_parser(grammar_rules rules, scanner_tokens tokens, const char *comment);
parser_t mk_parser_raw(const char *grammar, scanner s);
void destroy_parser(parser_t *g);
bool parse(parser_t *g, parse_context *ctx, AST **root, int start);
position_t get_position(const char *source, string_slice place);
void print_ast(AST *root);
void destroy_ast(AST *root);

typedef struct {
  char map[UINT8_MAX];
} terminal_list;

typedef struct {
  vec nonterminals_vec;
} nonterminal_list;

enum follow_type {
  FOLLOW_CHAR,
  FOLLOW_SYMBOL,
  FOLLOW_FIRST,
  FOLLOW_FOLLOW,
};

struct follow_t {
  enum follow_type type;
  char ch;
  regex *regex;
  const production_t *prod;
};


terminal_list get_terminals(const parser_t *g);
nonterminal_list get_nonterminals(const parser_t *g);
void populate_first(production_t *h);
void populate_follow(const parser_t *g);
bool is_ll1(const parser_t *g);
void graph_walk(symbol_t *start, vec *all);
vec populate_maps(const production_t *owner, vec follows);
void add_symbols(symbol_t *start, int k, vec *follows);

#endif  // !EBNF_H
