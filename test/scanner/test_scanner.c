// link scanner/scanner.o
// link regex.o arena.o collections.o logging.o
#include "../unittest.h"
#include "logging.h"
#include "macros.h"
#include "scanner/scanner.h"
#include "text.h"
#include <string.h>

void test_regex_scanner(void) {
  token_def token_definition[] = {
      {"string",        (char *)string_regex        },
      {"float",         "(\\d+\\.\\d*|\\d*\\.\\d+)f"},
      {"double",        "(\\d+\\.\\d*|\\d*\\.\\d+)" },
      {"integer",       "\\d+"                      },
      {"bool",          "true|false"                },
      {"comma",         ","                         },
      {"period",        "\\."                       },
      {"colon",         ":"                         },
      {"semicolon",     ";"                         },
      {"leftarrow",     "<-"                        },
      {"rightarrow",    "->"                        },
      {"fatrightarrow", "=>"                        },
      {"less-than",     "<"                         },
      {"greater-than",  ">"                         },
      {"div",           "/"                         },
      {"mod",           "%"                         },
      {"mult",          "\\*"                       },
      {"plus",          "\\+"                       },
      {"minus",         "-"                         },
      {"not-equals",    "!="                        },
      {"equals",        "=="                        },
      {"assign",        "="                         },
      {"unary_not",     "!"                         },
      {"complement",    "~"                         },
      {"lpar",          "\\("                       },
      {"rpar",          "\\)"                       },
      {"lsqbrk",        "\\["                       },
      {"rsqbrk",        "\\]"                       },
      {"lcbrk",         "{"                         },
      {"rcbrk",         "}"                         },
      {"identifier",    "[a-zA-Z_][a-zA-Z_0-9]*"    },
  };

  static char program[] = {
      "303* (404+2) ",
  };
  scanner s = mk_scanner(mk_tokens(token_definition));
  s.ctx = &mk_ctx(program);
  bool valid[LENGTH(token_definition)] = {0};
  if (next_token(&s, valid, NULL) != ERROR_TOKEN)
    die("Expected no valid tokens");

  string_slice tmp;
  if (next_token(&s, NULL, &tmp) != 3 /* int token */)
    die("Expected int token.");
  rewind_scanner(&s, tmp);

  for (int i = 0; i < LENGTH(token_definition); i++) {
    valid[i] = 1;
  }

  const char *expected[][2] = {
      {"integer", "303"},
      {"mult",    "*"  },
      {"lpar",    "("  },
      {"integer", "404"},
      {"plus",    "+"  },
      {"integer", "2"  },
      {"rpar",    ")"  },
  };

  for (int i = 0; i < LENGTH(expected); i++) {
    const char *type = expected[i][0];
    const char *content = expected[i][1];
    string_slice matched = {0};
    int next = next_token(&s, valid, &matched);
    if (next == EOF_TOKEN || next == ERROR_TOKEN) {
      error("Unexpected token %d:", next);
      error_ctx(s.ctx);
    }
    token *actual = vec_nth(&s.tokens.slice, next);

    if (strncmp(type, actual->name.str, actual->name.n) != 0) {
      error("Token %d type mismatch. Expected %s, got %s", i, type, actual->name);
    }
    int l = strlen(content);
    if (strncmp(content, matched.str, l) != 0) {
      error("Token %d value mismatch. Expected %s, got %.*s", i, content, matched.n, matched.str);
    }
  }
  if (next_token(&s, valid, NULL) != EOF_TOKEN)
    die("Expected EOF");
  destroy_scanner(&s);
}

int main(void) {
  test_regex_scanner();
  assert2(log_severity() <= INFO);
  return 0;
}
