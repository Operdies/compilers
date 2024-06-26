// link: collections.o logging.o
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "collections.h"
#include "logging.h"
#include "macros.h"
#include "regex.h"
#include "unittest.h"

int *negate(int *v) {
  static int i;
  i = *v;
  i = -i;
  return &i;
}

void add_one(int *v) { *v += 1; }

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
    int actual = *(int *)vec_nth(s, i);
    if (expected != actual)
      return 4;
  }

  vec v2 = vec_select(&v.slice, sizeof(void *), (vec_selector)negate);
  for (int i = 0; i < v2.n; i++) {
    int from = *(int *)vec_nth(v, i);
    int to = *(int *)vec_nth(v2, i);
    if (from != -to) {
      return 5;
    }
  }

  vec_foreach(&v.slice, (vec_fn)add_one);
  for (int i = 0; i < s.n; i++) {
    int expected = payload[i % LENGTH(payload)] + 1;
    int actual = *(int *)vec_nth(s, i);
    if (expected != actual)
      return 6;
  }

  vec_destroy(&v);
  vec_destroy(&v2);

  return 0;
}

int int_comparer(const void *a, const void *b) { return *(int *)a - *(int *)b; }

void test_vec_swap(void) {
  int size = 10000;
  vec v1 = v_make(int);
  vec_ensure_capacity(&v1, size);

  for (int i = 0; i < size; i++)
    vec_push(&v1, &i);
  for (int i = 0; i < size; i++)
    assert2(i == *(int *)(vec_nth(v1, i)));
  vec_reverse(&v1);
  for (int i = 0; i < size; i++)
    assert2(i == *(int *)(vec_nth(v1, size - i - 1)));

  vec_sort(&v1, int_comparer);

  for (int i = 0; i < size; i++)
    assert2(i == *(int *)(vec_nth(v1, i)));

  vec_destroy(&v1);
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
  assert2(slicecmp(s1, s2) != 0);
  s2.n += 1;
  assert2(slicecmp(s1, s2) == 0);
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
  v_foreach(int, val, v) {
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

void test_vec_format(void) {
  char buf1[200];
  char buf2[200];
  int n1;
  int n2;

#define aresame(fmt, ...)                                                                                    \
  n1 = sprintf(buf1, fmt, __VA_ARGS__);                                                                      \
  n2 = vec_write(&(vec){.n = 0, .sz = sizeof(buf2[0]), .c = LENGTH(buf2), .array = buf2}, fmt, __VA_ARGS__); \
  if (n1 != n2 || strncmp(buf1, buf2, n1) != 0)                                                              \
    die("Strings differ:\nExpected: '%s'\nActual:   '%s'", buf1, buf2);

  aresame("%+-3d", 5);
  aresame("%+-3d", -5);

  aresame("%%+-3d %d", 3);
  aresame("%+-3d %d", 3, -3);
  aresame("%+-3d %d", -3, 3);
  aresame("%-3s", "H");
  aresame("%-3s", "Hahahaha");
  aresame("%-3.*s", 4, "Hahahaha");
  aresame("%-3.*s %s", 20, "Hahahaha", "funmy");
  aresame("%3s", "H");
  aresame("%p", (void *)buf1);

  aresame("%.0s", "a string");
  aresame("%%3.3d %d", 1);

  aresame("%+5.3d", -3);
  aresame("%-2.30d", 123456);
  aresame("%-8.3d", -3);
  aresame("%3c", 'x');
}

int main(void) {
  setup_crash_stacktrace_logger();
  test_push_string();
  test_vec();
  test_vec_insert();
  test_slice_cmp();
  test_vec_swap();
  test_vec_format();
  assert2(log_severity() <= LL_INFO);
  return 0;
}
