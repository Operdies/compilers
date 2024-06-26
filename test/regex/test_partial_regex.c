// link: regex.o arena.o collections.o logging.o
#include "../unittest.h"
#include "logging.h"
#include "macros.h"
#include "regex.h"

typedef struct {
  char *pattern;
  char *string;
  int match_index;
} testcase;

int match(testcase *t) {
  regex *r1 = mk_regex(t->pattern);
  regex_match r = regex_pos(r1, t->string, 0);
  destroy_regex(r1);
  return r.matched.n;
}

int main(void) {
  testcase testcases[] = {
      {.pattern = "[0-9]+",   .string = "123.456",    .match_index = 3 },
      {.pattern = "[0-9]*",   .string = "123.456",    .match_index = 3 },
      {.pattern = "[0-9]+?",  .string = "123.456",    .match_index = 1 },
      {.pattern = "[0-9]*?",  .string = "123.456",    .match_index = 0 },
      {.pattern = "[0-9]*?",  .string = "123.456",    .match_index = 0 },
      {.pattern = ".*?ab",    .string = "123123abab", .match_index = 8 },
      {.pattern = ".*?.*?ab", .string = "123123abab", .match_index = 8 },
      {.pattern = ".*ab",     .string = "123123abab", .match_index = 10},
  };

  bool fail = false;
  for (int i = 0; i < LENGTH(testcases); i++) {
    testcase *t = &testcases[i];
    int idx = match(t);
    if (idx != t->match_index) {
      printf("test_greed failed:\nmatch %s on %s yielded index %d, expected %d\n", t->pattern, t->string, idx,
             t->match_index);
      fail = true;
    }
  }
  assert2(log_severity() <= LL_INFO);
  return fail ? 1 : 0;
}
