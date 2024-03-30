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

typedef struct factor_t factor_t;
typedef struct production_t production_t;
typedef struct term_t term_t;
typedef struct expression_t expression_t;
typedef struct identifier_t identifier_t;
typedef struct grammar_t grammar_t;
typedef struct token_t token_t;
typedef struct parser_t parser_t;
typedef struct position_t position_t;

struct term_t {
  union {
    vec v;
    struct {
      int n;
      factor_t *factors;
    };
  };
  string_slice range;
};

struct expression_t {
  union {
    vec v;
    struct {
      int n;
      term_t *terms;
    };
  };
  string_slice range;
};

struct identifier_t {
  string_slice name;
  production_t *production;
};

struct factor_t {
  union {
    identifier_t identifier;
    string_slice string;
    expression_t expression;
  };
  string_slice range;
  enum factor_switch type;
};

struct production_t {
  string_slice identifier;
  string_slice rule;
  string_slice src;
  expression_t expr;
};

struct grammar_t {
  union {
    vec v;
    struct {
      int n;
      production_t *productions;
    };
  };
  parse_context ctx;
};

struct token_t {
  string_slice name;
  string_slice value;
};

struct parser_t {
  union {
    vec v;
    struct {
      int n;
      token_t *tokens;
    };
  };
  grammar_t *g;
};

struct position_t {
  int line;
  int column;
};

grammar_t parse_grammar(const char *grammar);
void destroy_grammar(grammar_t *g);
position_t get_position(const char *source, string_slice place);
parser_t tokenize(const grammar_t *g, const char *body);

#endif // !EBNF_H
