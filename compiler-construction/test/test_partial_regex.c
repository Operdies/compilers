// link: regex.o arena.o
#include "regex.h"
#include "unittest.h"
#include "macros.h"

int match(char *ex, char *str){
  regex *r1 = mk_regex(ex);
  int r = regex_pos(r1, str, 0);
  destroy_regex(r1);
  return r;
}
void test_greed(void){
  struct {
    char *r;
    int idx;
  } testcases[] = {
    { .r = "[0-9]+", .idx = 3},
    { .r = "[0-9]*", .idx = 3},
    { .r = "[0-9]+?", .idx = 1},
    { .r = "[0-9]*?", .idx = 0},
  };

  bool fail = false;
  for (int i =0; i < LENGTH(testcases); i++){
    int idx = match(testcases[i].r, "123.456");
    if (idx != testcases[i].idx){
      printf("test_greed failed: %s was %d, expected %d\n", testcases[i].r, idx, testcases[i].idx);
      fail = true;
    }
  }
  ASSERT(!fail);
}

int main(void) {
  test_greed();
  return 0;
}
