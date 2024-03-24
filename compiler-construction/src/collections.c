#include "collections.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool mk_string(string *s, size_t initial_capacity) {
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

static size_t next_size(size_t capacity, size_t required) {
  if (capacity == 0)
    capacity = 1;
  while (capacity < required)
    capacity *= 2;
  return capacity;
}

static bool ensure_size(string *s, size_t required) {
  if (required > s->capacity) {
    char *old = s->data;
    size_t n = s->length;
    if (!mk_string(s, next_size(s->capacity, required))) {
      return false;
    }
    memcpy(s->data, old, n);
    s->length = n;
    free(old);
  }
  return true;
}

bool push_str(string *s, size_t n, char data[static n]) {
  if (s == NULL) return false;
  bool aliased, resize, realloc;
  size_t required;

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
