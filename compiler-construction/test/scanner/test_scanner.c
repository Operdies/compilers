// link scanner/scanner.o
// link regex.o arena.o collections.o logging.o
#include "logging.h"
#include "macros.h"
#include "scanner/scanner.h"
#include "text.h"

static char program[] = {
    "3*(4+2)",
};

int main(void) {
  token_def token_definition[] = {
      {"[ \n\t]+",                   "whitespace"   },
      {(char *)string_regex,         "string"       },
      {"(\\d+\\.\\d*|\\d*\\.\\d+)f", "float"        },
      {"(\\d+\\.\\d*|\\d*\\.\\d+)",  "double"       },
      {"\\d+",                       "integer"      },
      {",",                          "comma"        },
      {"\\.",                        "period"       },
      {":",                          "colon"        },
      {";",                          "semicolon"    },
      {"<-",                         "leftarrow"    },
      {"->",                         "rightarrow"   },
      {"=>",                         "fatrightarrow"},
      {"<",                          "less-than"    },
      {">",                          "greater-than" },
      {"/",                          "div"          },
      {"%",                          "mod"          },
      {"\\*",                        "mult"         },
      {"\\+",                        "plus"         },
      {"-",                          "minus"        },
      {"!=",                         "not-equals"   },
      {"==",                         "equals"       },
      {"=",                          "assign"       },
      {"!",                          "unary_not"    },
      {"~",                          "complement"   },
      {"\\(",                        "lpar"         },
      {"\\)",                        "rpar"         },
      {"\\[",                        "lbrace"       },
      {"\\]",                        "rbrace"       },
      {"{",                          "lbracket"     },
      {"}",                          "rbracket"     },
      {"true|false",                 "bool"         },
      {"[a-zA-Z_][a-zA-Z_0-9]*",     "identifier"   },
  };
  static char grammar[] = {
      "expression = term {(plus | minus) term} \n"
      "term       = factor {(div | mult) factor}\n"
      "factor     = integer | identifier | lpar expression rpar\n"};
  (void)grammar;
  scanner s = {0};
  vec tokens = v_make(token_t);
  mk_scanner(&s, LENGTH(token_definition), token_definition);
  tokenize(&s, program, &tokens);
  info("%-20s %s", "[type]", "[content]");
  v_foreach(token_t *, t, tokens) {
    if (t->id) {
      token *actual = vec_nth(&s.tokens.slice, t->id);
      info("%-20s %.*s", actual->name, t->value.n, t->value.str);
    }
  }
}
