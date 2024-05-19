// link ebnf/ebnf.o ebnf/analysis.o scanner/scanner.o
// link regex.o arena.o collections.o logging.o
// link json/json_parser.o

#include "json/json_parser.h"
#include "macros.h"

static const char input[] = {
#include "large.json"
};

int main(void) {
  struct json_formatter p = mk_json_formatter();
  p.pretty = true;
  p.parser.recursive = false;
  format_buffer(&p, LENGTH(input), input, stdout);
  destroy_parser(&p.parser);
  return 0;
}
