#ifndef EBNF_H
#define EBNF_H

#include "collections.h"
#include "text.h"

// * factor     = identifier | string | "(" expression ")" | "[" expression "]" | "{" expression "}".
enum factor_switch {
  F_ERROR,
  F_OPTIONAL, // [ expression ]
  F_REPEAT,   // { expression }
  F_PARENS,   // ( expression )
  F_IDENTIFIER,
  F_STRING,
};

// shorthand for declaring a growable vector
#define DECLARE_VEC(type, varname) \
  union {                          \
    vec varname##_vec;             \
    struct {                       \
      int n_##varname;             \
      type *varname;               \
    };                             \
  }

typedef struct Symbol Symbol;
typedef struct Header Header;
typedef struct factor_t factor_t;
typedef struct production_t production_t;
typedef struct term_t term_t;
typedef struct expression_t expression_t;
typedef struct identifier_t identifier_t;
typedef struct parser_t parser_t;
typedef struct token_t token_t;
typedef struct position_t position_t;

struct term_t {
  string_slice range;
  DECLARE_VEC(factor_t, factors);
};

struct expression_t {
  string_slice range;
  DECLARE_VEC(term_t, terms);
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
  };
  enum factor_switch type;
};

struct production_t {
  // the identifier name
  string_slice identifier;
  // the parsed expression
  struct expression_t expr;
  // Header used in the parsing table
  Header *header;
};

struct parser_t {
  parse_context ctx;
  arena *a;
  string body;
  string error;
  DECLARE_VEC(struct production_t, productions);
};

struct Symbol {
  char sym;
  Symbol *next;
  Symbol *alt;
  production_t *Nonterminal;
  bool non_terminal;
  bool empty;
};

struct position_t {
  int line;
  int column;
};

struct token {
  string_slice name;
  string_slice value;
};

typedef struct {
  DECLARE_VEC(struct token, tokens);
} tokens;

parser_t mk_parser(const char *grammar);
void destroy_parser(parser_t *g);
tokens parse(parser_t *g, const char *program);
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
  DECLARE_VEC(char, terminals);
} terminal_list;

typedef struct {
  DECLARE_VEC(struct Header, nonterminals);
} nonterminal_list;

struct Header {
  production_t *prod;
  Symbol *sym;
  DECLARE_VEC(char, first);
  DECLARE_VEC(char, follow);
};

terminal_list get_terminals(const parser_t *g);
nonterminal_list get_nonterminals(const parser_t *g);
void populate_first(const parser_t *g, struct Header *h);
void populate_follow(const parser_t *g, struct Header *h);
bool is_ll1(const parser_t *g);

#endif // !EBNF_H
