// link: regex.o arena.o collections.o logging.o
#include "../unittest.h"
#include "macros.h"
#include "regex.h"
#include "text.h"

typedef struct {
  const char *pattern;
  char *string;
  bool match;
  int start;
  int length;
} testcase;

regex_match match(testcase *t) {
  regex *r1 = mk_regex(t->pattern);
  regex_match r = regex_find(r1, t->string);
  destroy_regex(r1);
  return r;
}

int main(void) {
  testcase ts[] = {
      {.pattern = string_regex, .string = "quote \"\\\"\"", .match = true, .start = 6, .length = 4},
      {.pattern = ".*ab", .string = "hello abcd", .match = true, .start = 0, .length = 8},
      {.pattern = "ble.*ab", .string = "hello abcd", .match = false, .start = 0, .length = 0},
      {.pattern = "ble.*ab", .string = "asdf blegab", .match = true, .start = 5, .length = 6},
      {.pattern = "ble.*ab", .string = "asdf blegab", .match = true, .start = 5, .length = 6},
      {.pattern = "\"[^\"]*\"", .string = "\"str\" \"other str\"", .match = true, .start = 0, .length = 5},
      {.pattern = "\"[^\"]*\"", .string = "\"str \\\"escaped!\"", .match = true, .start = 0, .length = 7},
      {.pattern = string_regex, .string = "empty \"\"", .match = true, .start = 6, .length = 2},
      {.pattern = string_regex, .string = "ab \"runaway string", .match = false},
      {.pattern = string_regex, .string = "ab \"runaway string \\\" 2", .match = false},
      {.pattern = string_regex, .string = "leading \"str \\\"escaped!\" rest", .match = true, .start = 8, .length = 16},
      {.pattern = string_regex, .string = "ab \"str \\\"escaped!\" rest", .match = true, .start = 3, .length = 16},
  };
  for (int i = 0; i < LENGTH(ts); i++) {
    testcase *t = &ts[i];
    regex_match m = match(t);
    if (t->match != m.match) {
      printf("test failed: match '%s' on '%s'\nyielded  %s\nexpected %s\n", t->pattern, t->string,
             m.match ? "match" : "no match", t->match ? "match" : "no match");
      if (m.match) {
        printf("matched: %.*s\n", m.matched.n, m.matched.str);
      }
    } else if (t->match && ((t->string + t->start) != m.matched.str || t->length != m.matched.n)) {
      printf("test failed: match '%s' on '%s'\nyielded  %.*s\nexpected %.*s\n", t->pattern, t->string, m.matched.n,
             m.matched.str, (int)t->length, t->string + t->start);
    }
  }
  assert2(log_severity() <= LL_INFO);
  return 0;
}
