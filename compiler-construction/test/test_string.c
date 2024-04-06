// link: collections.o
#include "collections.h"
#include "macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int *negate(int *v) {
  static int i;
  i = *v;
  i = -i;
  return &i;
}

void add_one(int *v) {
  *v += 1;
}

int test_vec(void) {
  vec v = {0};
  int payload[] = {7, 9, 13};
  mk_vec(&v, sizeof(int), 1);

  for (int i = 0; i < LENGTH(payload); i++)
    vec_push(&v, &payload[i]);

  int doublings = 3;
  for (int n = 0; n < doublings; n++) {
    if (!vec_push_slice(&v, &v.slice))
      return 2;
  }

  int expected_count = 24;
  vslice s = vec_slice(&v, 0, -1);
  if (s.n != expected_count)
    return 3;

  for (int i = 0; i < s.n; i++) {
    int expected = payload[i % LENGTH(payload)];
    int *actual = (int *)vec_nth(&s, i);
    if (!actual || expected != *actual)
      return 4;
  }

  vec v2 = vec_select(&v.slice, sizeof(void *), (vec_selector)negate);
  for (int i = 0; i < v2.n; i++) {
    int from = *(int *)vec_nth(&v.slice, i);
    int to = *(int *)vec_nth(&v2.slice, i);
    if (from != -to) {
      return 5;
    }
  }

  vec_foreach(&v.slice, (vec_fn)add_one);
  for (int i = 0; i < s.n; i++) {
    int expected = payload[i % LENGTH(payload)] + 1;
    int actual = *(int *)vec_nth(&s, i);
    if (expected != actual)
      return 6;
  }

  vec_destroy(&v);
  vec_destroy(&v2);

  return 0;
}

int main(void) {
  string s = {0};
  char *something = "something";
  mk_string(&s, 1);
  if (s.chars == NULL) {
    return -1;
  }
  push_str(&s, strlen(something), something);
  for (int j = 0; j < 10; j++) {
    push_str(&s, s.n, s.chars);
  }
  destroy_string(&s);

  return test_vec();
}
