// link ebnf/ebnf.o ebnf/analysis.o scanner/scanner.o
// link regex.o arena.o collections.o logging.o
#include <stdio.h>
#include <string.h>

#include "ebnf/ebnf.h"
#include "logging.h"
#include "macros.h"
#include "text.h"

#define tok(key, pattern) [key] = {#key, (char *)pattern}
enum json_tokens {
  string,
  number,
  boolean,
  comma,
  colon,
  lsqbrk,
  rsqbrk,
  lcbrk,
  rcbrk,
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
    tok(lsqbrk, "\\["),
    tok(rsqbrk, "\\]"),
    tok(lcbrk, "{"),
    tok(rcbrk, "}"),
};
const rule_def rules[] = {
    tok(object,
        "( lcbrk keyvalues rcbrk | lsqbrk list rsqbrk | number | "
        "string | boolean )"),
    tok(list, "[ object { comma object } ] "),
    tok(keyvalues, "[ keyvalue { comma keyvalue } ]"),
    tok(keyvalue, "string colon object"),
};

static bool pretty = true;
static bool recursive = false;

void visit(AST *a, int indent) {
#define print(a) printf("%.*s", a->range.n, a->range.str)
#define pprint(...)      \
  if (pretty) {          \
    printf(__VA_ARGS__); \
  }

  for (; a; a = a->next) {
    enum json_tokens node = a->node_id;
    switch (node) {
      case string:
      case number:
      case boolean:
      case comma:
      case colon:
        print(a);
        if (node == colon) {
          pprint(" ");
        } else if (node == comma) {
          pprint("\n%*s", indent, " ");
        }
        break;
      case lsqbrk:
      case lcbrk:
        indent += 2;
        print(a);
        pprint("\n%*s", indent, " ");
        break;
      case rsqbrk:
      case rcbrk:
        indent -= 2;
        pprint("\n%*s", indent, "");
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
#undef print
}

void _format(parse_context *ctx) {
  parser_t p = mk_parser(mk_rules(rules), mk_tokens(json_tokens));
  p.recursive = recursive;
  AST *a;
  if (parse(&p, ctx, &a, object)) {
    visit(a, 0);
    destroy_ast(a);
  } else {
    error_ctx(p.s->ctx);
  }
  destroy_parser(&p);
}

void format(FILE *f) {
  vec buf = v_make(char);
  vec_fcopy(&buf, f);
  parse_context ctx = {
      .view = {.str = buf.array, .n = buf.n}
  };
  _format(&ctx);
  vec_destroy(&buf);
}

int main(int argc, char **argv) {
  set_loglevel(LL_INFO);
  FILE *f = stdin;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-c") == 0)
      pretty = false;
    else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--recursive") == 0) {
      char *recstr = argv[i+1];
      recursive = strcmp(recstr, "true") == 0;
      i++;
    }
    else {
      f = fopen(argv[i], "r");
      if (!f)
        die("Error opening file %s:", argv[i]);
      break;
    }
  }
  format(f);
  fclose(f);
  return 0;
}
