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

struct Header {
  production_t *prod;
  Symbol *sym;
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

#endif // !EBNF_H
