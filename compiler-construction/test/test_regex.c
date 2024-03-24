#include "regex.h"
#include <stdio.h>
#include <unistd.h>

typedef struct {
  char *pattern;
  char *test;
  bool match;
} pair;

int main(void) {
  pair testcases[] = {
      {"a|b*", "a", true},
      {"a|b*", "", true},
      {"a|b*", "b", true},
      {"abc.def.*ghi", "abcidefasdfghi", true},
      {"abc.def.*ghi", "abcidefasdfghig", false},
      {"a|b*", "bb", true},
      {"def", "def", true},
      {"ghi", "ged", false},
      {"", "", true},
      {".", "", false},
      {".", "x", true},
      {"\\.", "x", false},
      {"\\.", ".", true},
      {"a", ".", false},
      {"(a|b)*c", "ac", true},
      {"(a|b)*c", "babbac", true},
      {"(a|b)*c", "babbab", false},
      {"(a|b)*c", "c", false},
  };

  for (int i = 0; i < (int)(sizeof(testcases) / sizeof(pair)); i++) {
    pair *p = &testcases[i];
    bool is_match = match(p->pattern, p->test);
    if (is_match != p->match) {
      char *strings[] = {"false", "true"};
      printf("Regex error: ");
      printf("Match %4s -> %4s\nShould be %s, was %s\n", p->pattern, p->test,
             strings[p->match], strings[is_match]);
    }
  }
  return 0;
}
