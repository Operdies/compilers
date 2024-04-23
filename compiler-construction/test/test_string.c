// link: collections.o logging.o
#include "collections.h"
#include "logging.h"
#include "macros.h"
#include <assert.h>
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
  vec v = v_make(int);
  int payload[] = {7, 9, 13};

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

void test_vec_write(void) {
  vec v = v_make(char);
  vec_write(&v, "Hello %d %s\n", 1, "guy");
  vec_write(&v, "Hello %d %s\n", 2, "bro");
  vec_push(&v, &(char){0});
  if (strcmp("Hello 1 guy\nHello 2 bro\n", v.array) != 0)
    die("vec write broken");
  vec_destroy(&v);
}

void test_push_string(void) {
  string_t s = {0};
  char *something = "something";
  mk_string(&s, 1);
  push_str(&s, strlen(something), something);
  int cnt = 3;
  for (int j = 0; j < cnt; j++) {
    push_str(&s, s.n, s.chars);
  }

  destroy_string(&s);
}

void test_slice_cmp(void) {
  char *str1 = "abcdefg";
  string_slice s1 = mk_slice(str1);
  string_slice s2 = mk_slice(str1);
  s1.n -= 1;
  s2.n -= 1;

  s2.n -= 1;
  assert(slicecmp(s1, s2) != 0);
  s2.n += 1;
  assert(slicecmp(s1, s2) == 0);
}

void test_vec_insert(void) {
  vec v = v_make(int);
  vec_insert(&v, 0, &(int){1});
  vec_insert(&v, 1, &(int){4});
  vec_insert(&v, 1, &(int){3});
  vec_insert(&v, 1, &(int){2});
  vec_insert(&v, 0, &(int){0});

  if (v.n != 5)
    die("vec insert failed: expected 5 elements");
  v_foreach(int *, val, v) {
    if (*val != idx_val)
      die("vec insert failed: expected %d, got %d", idx_val, *val);
  }
  vec_clear(&v);

  for (int i = 0; i <= 1000; i++) {
    vec_insert(&v, 0, &i);
  }
  v_foreach((void), val, v) {
    int expected = 1000 - idx_val;
    if (*val != (expected))
      die("vec insert failed: expected %d, got %d", expected, *val);
  }

  vec_destroy(&v);
}

int main(void) {
  setup_crash_stacktrace_logger();
  test_push_string();
  test_vec_write();
  test_vec();
  test_vec_insert();
  test_slice_cmp();
  return 0;
}
