#include "collections.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mk_string(string *s, int initial_capacity) {
  mk_vec(&s->v, 1, initial_capacity);
}

void destroy_string(string *s) { vec_destroy(&s->v); }

void push_str(string *s, int n, const char data[static n]) {
  if (s) {
    vslice vec = {.n = n, .sz = 1, .arr = (char *)data};
    vec_push_slice(&s->v, &vec);
  }
}

void mk_vec(vec *v, int elem_size, int initial_capacity) {
  void *data = ecalloc(initial_capacity, elem_size);
  v->n = 0;
  v->c = initial_capacity;
  v->sz = elem_size;
  v->array = data;
}

static void ensure_capacity(vec *v, int c) {
  if (v->c >= c)
    return;
  if (v->c <= 0)
    v->c = 1;
  while (v->c < c)
    v->c *= 2;
  void *new_data = reallocarray(v->array, v->c, v->sz);
  if (!new_data)
    die("reallocarray:");
  v->array = new_data;
}

void vec_push(vec *v, void *elem) {
  ensure_capacity(v, v->n + 1);
  char *addr = (char *)v->array;
  memmove(addr + v->n * v->sz, elem, v->sz);
  v->n++;
}

void *vec_pop(vec *v) {
  if (v->n <= 0) return NULL;
  void *val = vec_nth(&v->slice, v->n - 1);
  v->n -= 1;
  return val;
}

bool vec_push_slice(vec *v, const vslice *s) {
  if (!v || !s)
    return false;
  if (v->sz != s->sz)
    return false;
  ensure_capacity(v, v->n + s->n);

  char *dest = (char *)v->array + v->n * v->sz;
  char *src = (char *)s->arr;
  memmove(dest, src, s->n * s->sz);

  // Optimization: Copy non-overlapping memory regions with memcpy

  v->n += s->n;
  return true;
}

static int roll(int x, int n) {
  if (x < 0) {
    x = -x;
    x = n - x;
    x++;
  }
  return x;
}

vslice vec_slice(vec *v, int start, int end) {
  vslice no_slice = {0};
  if (v) {
    start = roll(start, v->n);
    end = roll(end, v->n);
    if (start < 0 || end < 0 || start > end)
      return no_slice;
    return (vslice){
        .sz = v->sz, .n = end - start, .arr = (char *)v->array + start * v->sz};
  }
  return no_slice;
}

void *vec_nth(const vslice *v, int n) {
  if (!v)
    return NULL;
  n = roll(n, v->n);
  if (n >= v->n)
    return NULL;
  char *addr = (char *)v->arr;
  return addr + n * v->sz;
}

void vec_clear(vec *v) { v->n = 0; }

void vec_destroy(vec *v) {
  free(v->array);
  *v = (vec){0};
}

void vec_foreach(vslice *v, vec_fn f) {
  if (v) {
    for (int i = 0; i < v->n; i++) {
      void *elem = (char *)v->arr + i * v->sz;
      f(elem);
    }
  }
}

vec vec_select(const vslice *v, int elem_size, vec_selector s) {
  vec result = {0};
  if (v) {
    mk_vec(&result, elem_size, v->n);
    for (int i = 0; i < v->n; i++) {
      void *elem = (char *)v->arr + i * v->sz;
      void *new_elem = s(elem);
      vec_push(&result, new_elem);
    }
  }
  return result;
}

int slicecmp(string_slice s1, string_slice s2) {
  int shortest = s1.n > s2.n ? s2.n : s1.n;
  return strncmp(s1.str, s2.str, shortest);
}

static void *identity(void *v) { return v; }
vec vec_clone(const vec *v) { return vec_select(&v->slice, v->sz, identity); }

string string_from_chars(const char *src, int n) {
  string s = {0};
  mk_string(&s, n);
  push_str(&s, n, src);
  return s;
}

void push_char(string *s, char ch) { vec_push(&s->v, &ch); }
bool string_contains(const string *s, char ch) {
  for (int i = 0; i < s->n; i++) {
    if (s->chars[i] == ch)
      return true;
  }
  return false;
}

bool vec_contains(const vec *v, const void *elem) {
  return vslice_contains(&v->slice, elem);
}

bool vslice_contains(const vslice *v, const void *elem) {
  for (int i = 0; i < v->n; i++) {
    const void *item = vec_nth(v, i);
    if (memcmp(item, elem, v->sz) == 0)
      return true;
  }
  return false;
}

void die(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
    fputc(' ', stderr);
    perror(NULL);
  } else {
    fputc('\n', stderr);
  }

  exit(1);
}

void *ecalloc(size_t nmemb, size_t size) {
  void *p;

  if (!(p = calloc(nmemb, size)))
    die("calloc:");
  return p;
}
