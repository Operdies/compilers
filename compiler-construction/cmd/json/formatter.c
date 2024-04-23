// link ebnf/ebnf.o ebnf/analysis.o scanner/scanner.o
// link regex.o arena.o collections.o logging.o
#include "ebnf/ebnf.h"
#include "logging.h"
#include "macros.h"
#include "text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define tok(key, pattern) [key] = {#key, \
                                   (char *)pattern}
enum json_tokens {
  string,
  number,
  boolean,
  comma,
  colon,
  lbrace,
  rbrace,
  lbracket,
  rbracket,
  object,
  list,
  keyvalues,
  keyvalue
};

static token_def json_tokens[] = {
    tok(string, string_regex),
    tok(number, "-?(\\d+|\\d+\\.\\d*|\\d*\\.\\d+)"),
    tok(boolean, "true|false"),
    tok(comma, ","),
    tok(colon, ":"),
    tok(lbrace, "\\["),
    tok(rbrace, "\\]"),
    tok(lbracket, "{"),
    tok(rbracket, "}"),
};
const struct rule_def rules[] = {
    tok(object, "( lbracket keyvalues rbracket | lbrace list rbrace | number | string | boolean )"),
    tok(list, "[ object { comma object } ] "),
    tok(keyvalues, "[ keyvalue { comma keyvalue } ]"),
    tok(keyvalue, "string colon object"),
};

void visit(AST *a, int indent) {
#define print(a)                            \
  printf("%.*s", a->range.n, a->range.str); \
  fflush(stdout);

  for (; a; a = a->next) {
    enum json_tokens node = a->node_id;
    switch (node) {
    case string:
    case number:
    case boolean:
    case comma:
    case colon:
      print(a);
      if (node == colon)
        printf(" ");
      else if (node == comma)
        printf("\n%*s", indent, " ");
      break;
    case lbrace:
    case lbracket:
      indent += 2;
      print(a);
      printf("\n%*s", indent, " ");
      break;
    case rbrace:
    case rbracket:
      indent -= 2;
      printf("\n%*s", indent, "");
      print(a);
      break;
    case object:
    case list:
    case keyvalues:
    case keyvalue:
      break;
    }
    if (a->first_child)
      visit(a->first_child, indent);
  }
}

void _format(parse_context *ctx) {
  scanner s = {0};
  mk_scanner(&s, LENGTH(json_tokens), json_tokens);
  parser_t p = mk_parser(LENGTH(rules), rules, &s);
  AST *a;
  if (parse(&p, ctx, &a, object))
    // print_ast(a, NULL);
    visit(a, 0);
  destroy_ast(a);
  destroy_parser(&p);
  destroy_scanner(&s);
}

void format(FILE *f) {
  vec buf = v_make(char);
  vec_fcopy(&buf, f);
  parse_context ctx = {.src = buf.array, .n = buf.n};
  _format(&ctx);
  vec_destroy(&buf);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    format(stdin);
  } else {
    for (int i = 1; i < argc; i++) {
      char *filename = argv[i];
      FILE *f = fopen(filename, "r");
      format(f);
      fclose(f);
    }
  }
  return 0;
}
