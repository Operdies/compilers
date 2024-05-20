// link ebnf/ebnf.o ebnf/analysis.o scanner/scanner.o
// link regex.o arena.o collections.o logging.o
// link json/json_parser.o
#include <stdio.h>
#include <string.h>
#include "json/json_parser.h"
#include "logging.h"


int main(int argc, char **argv) {
  bool pretty;
  pretty = true;

  set_loglevel(LL_INFO);
  FILE *f = stdin;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-c") == 0)
      pretty = false;
    else {
      f = fopen(argv[i], "r");
      if (!f)
        die("Error opening file %s:", argv[i]);
      break;
    }
  }

  struct json_formatter p = mk_json_formatter();
  p.pretty = pretty;
  format_file(&p, f, stdout);
  fclose(f);
  destroy_parser(&p.parser);
  return 0;
}
