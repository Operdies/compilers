// link: regex.o arena.o
#include "macros.h"
#include "regex.h"

typedef struct {
  char *pattern;
  char *string;
  bool match;
  size_t start;
  size_t length;
} testcase;

regex_match match(testcase *t) {
  regex *r1 = mk_regex(t->pattern);
  regex_match r = regex_find(r1, t->string);
  destroy_regex(r1);
  return r;
}

int main(void) {
  testcase ts[] = {
      {.pattern = ".*ab",    .string = "hello abcd",  .match = true,  .start = 0, .length = 8},
      {.pattern = "ble.*ab", .string = "hello abcd",  .match = false, .start = 0, .length = 0},
      {.pattern = "ble.*ab", .string = "asdf blegab", .match = true,  .start = 5, .length = 6},
  };
  for (int i = 0; i < LENGTH(ts); i++) {
    testcase *t = &ts[i];
    regex_match m = match(t);
    if (t->match != m.match) {
      printf("test failed: match '%s' on '%s\nyielded  %s\nexpected %s\n", t->pattern, t->string, m.match ? "match" : "no match", t->match ? "match" : "no match");
    } else if (t->match && (t->start != m.start || t->length != m.length)) {
      printf("test failed: match '%s' on '%s\nyielded  %.*s\nexpected %.*s\n", t->pattern, t->string, (int)m.length, t->string + m.start,
             (int)t->length, t->string + t->start);
    }
  }
  return 0;
}
