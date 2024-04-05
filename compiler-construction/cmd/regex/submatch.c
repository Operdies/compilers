// link: regex.o arena.o
#include "regex.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[argc + 1]) {
  for (int i = 1; (i + 1) < argc; i += 2) {
    char *pattern = argv[i];
    char *test = argv[i + 1];
    regex *r = mk_regex(pattern);
    if (!r) {
      printf("Invalid pattern: %s\n", pattern);
      continue;
    }
    regex_match m = regex_find(r, test);
    if (m.match) {
      printf("%.*s\n", (int)m.length, test + m.start);
    } else {
      fprintf(stderr, "No match.\n");
    }
  }
  return 0;
}