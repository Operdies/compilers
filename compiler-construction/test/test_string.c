// link: collections.o
#include "collections.h"
#include "macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int test_vec(void) {
  vec v;
  int payload[] = {7, 9, 13};
  if (!mk_vec(&v, sizeof(int), 1))
    return 1;

  for (int i = 0; i < LENGTH(payload); i++)
    if (!vec_push(&v, &payload[i]))
      return 1;

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
    int actual = *(int *)vec_nth(&s, i);
    if (expected != actual)
      return 4;
  }
  return 0;
}

int main(void) {
  string s = {0};
  char *something = "something";
  mk_string(&s, 1);
  if (s.data == NULL) {
    return -1;
  }
  push_str(&s, strlen(something), something);
  for (int j = 0; j < 10; j++) {
    push_str(&s, s.length, s.data);
  }
  destroy_string(&s);

  return test_vec();
}
