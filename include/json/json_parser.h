#pragma once

#include <stdio.h>
#include "ebnf/ebnf.h"

struct json_formatter {
  parser_t parser;
  bool pretty;
};

struct json_formatter mk_json_formatter(void);
void format_file(struct json_formatter *p, FILE *in, FILE *out);
void format_buffer(struct json_formatter *p, int n, const char buffer[static n], FILE *out);
