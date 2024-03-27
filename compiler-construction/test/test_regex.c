// link: regex.o arena.o
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
      // simple test cases
      {"h+*", "hhh", false},
      {"h+", "h", true},
      {"h+", "", false},
      {"h+", "hh", true},
      {"h+", "hhh", true},
      {"\\(", "(", true},
      {"[a-e]", "a", true},
      {"[a-e]", "e", true},
      {"[a-e]", "f", false},
      {"[a-e0-3]", "1", true},
      {"[a-e0-3]", "a", true},
      {"[a-e0-3]", "4", false},
      {"[a-z][a-zA-Z]*[a-z]", "hHELLO", false},
      {"[a-z][a-zA-Z]*[a-z]", "hHELLo", true},
      {"a?", "a", true},
      {"a?", "", true},
      {"a?", "aa", false},
      {"a?", "b", false},
      {"a?", "ab", false},
      {"a?", "ba", false},
      {"a?b", "b", true},
      {"a?b", "ab", true},
      {"ba?", "b", true},
      {"ba?", "ba", true},
      {"ab?c", "ac", true},
      {"ab?c", "abc", true},
      {"ab?c", "c", false},
      {"(abc[de])?f", "f", true},
      {"(abc[de])?f", "abcef", true},
      {"(abc[de])?f", "abcf", false},
      {"(abc[de])?f", "abcdf", true},
      {"(abc[de])?f", "abcd", false},
      {"(abc[de]?)?f", "abcdf", true},
      {"(abc[de]?)?f", "abcf", true},
      {"(abc[de]?)?f", "abc", false},
      {"(a|)c", "ac", true},
      {"(a|b)*c", "ac", true},
      {"(a|b)*c", "bc", true},
      {"(a|b)*c", "c", true},
      {"(a|b)*?c", "babbac", true},
      {"(a|b)*?c", "babbab", false},
      {"(a|b)*c", "babbac", true},
      {"(a|b)*c", "babbab", false},
      {"", "", true},
      {".", "", false},
      {".", "x", true},
      {"[ab][cd]", "ac", true},
      {"[ab][cd]", "bc", true},
      {"[ab][cd]", "ad", true},
      {"[ab][cd]", "bd", true},
      {"[ab][cd][ef]", "acf", true},
      {"[ab][cd][ef]", "acg", false},
      {"", "", true},
      {"", "a", false},
      {"abab", "abab", true},
      {"abab", "aba", false},
      {"[ab]", "a", true},
      {"[ab]", "a", true},
      {"[ab]", "b", true},
      {"[ab]", "c", false},
      {"ab|cd", "ab", true},
      {"ab|cd", "cd", true},
      {"ab|cd", "acd", false},
      {"ab|cd", "a", false},
      {"ab|cd", "bcd", false},
      {"(ab|cd)", "ab", true},
      {"(ab|cd)", "cd", true},
      {"(ab|cd)", "acd", false},
      {"(ab|cd)", "a", false},
      {"(ab|cd)", "bcd", false},
      {"((ab)*|cd)", "ababab", true},
      {"((ab)*?|cd)", "ababab", true},
      {"((ab)*|cd)", "cd", true},
      {"a|b*", "a", true},
      {"a|b*", "", true},
      {"a|b*", "b", true},
      {"a|b*", "bb", true},
      {"\\.", "x", false},
      {"\\.", ".", true},
      {"a", ".", false},
      {"abc.def.*ghi", "abcidefasdfghi", true},
      {"abc.def.*ghi", "abcidefasdfghig", false},
      {"abc.def.*?ghi", "abcidefasdfghig", false},
      {"a*b*c", "aaaaaaaac", true},
      {"a*?b*?c", "aaaaaaaac", true},
  };

  int status = 0;
  int n = (int)(sizeof(testcases) / sizeof(pair));
  for (int i = 0; i < n; i++) {
    pair *p = &testcases[i];
    bool is_match = matches(p->pattern, p->test);
    if (is_match != p->match) {
      char *strings[] = {"false", "true"};
      char buf1[50];
      char buf2[50];
      sprintf(buf1, "'%s'", p->pattern);
      sprintf(buf2, "'%s'", p->test);
      printf("Match %4s\n      %4s\n"
             "Expect %s\n    is %s\n",
             buf1, buf2, strings[p->match], strings[is_match]);
      status++;
    }
  }
  // printf("%d / %d tests passing.\n", n - status, n);
  return status;
}
