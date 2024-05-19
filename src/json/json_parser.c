#include "json/json_parser.h"

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

static void visit(AST *a, int indent, FILE *out, bool pretty) {
#define print(a) fprintf(out, "%.*s", a->range.n, a->range.str)
#define pprint(...)            \
  if (pretty) {                \
    fprintf(out, __VA_ARGS__); \
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
      visit(a->first_child, indent, out, pretty);
  }
#undef print
}

static void format(struct json_formatter *p, parse_context *ctx, FILE *out) {
  AST *a;
  if (parse(&p->parser, ctx, &a, object)) {
    visit(a, 0, out, p->pretty);
    destroy_ast(a);
  } else {
    error_ctx(&p->parser.ctx);
  }
}

struct json_formatter mk_json_formatter(void) {
  struct json_formatter p = {0};
  p.parser = mk_parser(mk_rules(rules), mk_tokens(json_tokens));
  return p;
}

void format_buffer(struct json_formatter *p, int n, const char buffer[static n], FILE *out) {
  parse_context ctx = {
      .view = {.str = buffer, .n = n}
  };
  format(p, &ctx, out);
}

void format_file(struct json_formatter *p, FILE *in, FILE *out) {
  vec buf = v_make(char);
  vec_fcopy(&buf, in);
  format_buffer(p, buf.n, buf.array, out);
  vec_destroy(&buf);
}
