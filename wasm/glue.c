#include <string.h>

#include "collections.h"
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
static void visit(AST *a, int indent, vec *output) {
#define print(a) vec_write(output, "%.*s", a->range.n, a->range.str)
#define pprint(...)      \
  if (pretty) {          \
    vec_write(output, __VA_ARGS__); \
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
      visit(a->first_child, indent, output);
  }
#undef print
}

static void _format(parse_context *ctx, char *result) {
  parser_t p = mk_parser(mk_rules(rules), mk_tokens(json_tokens));
  AST *a;
  if (parse(&p, ctx, &a, object)) {
    vec output = v_make(char);
    visit(a, 0, &output);
    strncpy(result, output.array, output.n);
    result[output.n] = 0;
    vec_destroy(&output);
    destroy_ast(a);
  } else {
    error_ctx(p.s->ctx);
  }
  destroy_parser(&p);
}

void format(const char *input, char *output, int n) {
  parse_context ctx = {.src = input, .n = n};
  _format(&ctx, output);
}
