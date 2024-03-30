#include "collections.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool mk_string(string *s, int initial_capacity) {
  char *data = calloc(1, initial_capacity);
  if (data != NULL) {
    s->length = 0;
    s->capacity = initial_capacity;
    s->data = data;
    return true;
  }
  return false;
}

void destroy_string(string *s) { free(s->data); }

static int next_size(int capacity, int required) {
  if (capacity == 0)
    capacity = 1;
  while (capacity < required)
    capacity *= 2;
  return capacity;
}

static bool ensure_size(string *s, int required) {
  if (required > s->capacity) {
    char *old = s->data;
    int n = s->length;
    if (!mk_string(s, next_size(s->capacity, required))) {
      return false;
    }
    memcpy(s->data, old, n);
    s->length = n;
    free(old);
  }
  return true;
}

bool push_str(string *s, int n, char data[static n]) {
  if (s == NULL)
    return false;
  bool aliased, resize, realloc;
  int required;

  if (data == NULL)
    return false;
  required = s->length + n + 1;

  aliased = data >= s->data && data <= s->data + s->length;
  resize = s->capacity < required;
  realloc = aliased && resize;

  if (realloc)
    data = strdup(data);
  if (data == NULL)
    return false;

  bool success = ensure_size(s, required);
  if (success) {
    memcpy(s->data + s->length, data, n);
    s->length += n;
    s->data[s->length] = 0;
  }
  if (realloc)
    free(data);
  return true;
}

bool mk_vec(vec *v, int elem_size, int initial_capacity) {
  void *data = calloc(initial_capacity, elem_size);
  if (!data)
    return false;
  v->n = 0;
  v->c = initial_capacity;
  v->sz = elem_size;
  v->arr = data;
  return true;
}

static bool ensure_capacity(vec *v, int c) {
  if (v->c >= c)
    return true;
  if (v->c <= 0)
    v->c = 1;
  while (v->c < c)
    v->c *= 2;
  void *new_data = reallocarray(v->arr, v->c, v->sz);
  if (!new_data)
    return false;
  v->arr = new_data;
  return true;
}

bool vec_push(vec *v, void *elem) {
  if (!ensure_capacity(v, v->n + 1))
    return false;
  char *addr = (char *)v->arr;
  memmove(addr + v->n * v->sz, elem, v->sz);
  v->n++;
  return true;
}

bool vec_push_slice(vec *v, const vslice *s) {
  if (!v || !s)
    return false;
  if (v->sz != s->sz)
    return false;
  if (!ensure_capacity(v, v->n + s->n))
    return false;

  char *dest = (char *)v->arr + v->n * v->sz;
  char *src = (char *)s->arr;
  memmove(dest, src, s->n * s->sz);

  // Optimization: Copy non-overlapping memory regions with memcpy

  v->n += s->n;
  return true;
}

int roll(int x, int n) {
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
    return (vslice){.sz = v->sz, .n = end - start, .arr = (char *)v->arr + start * v->sz};
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

void vec_destroy(vec *v) {
  free(v->arr);
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
