#include "arena.h"
#include "collections.h"
#include <stdlib.h>
#include <unistd.h>

static long __pagesize = 0;
#define PAGESIZE (__pagesize ? __pagesize : (__pagesize = sysconf(_SC_PAGESIZE)))

static arena *mk_arena_sized(size_t size) {
  arena *a;
  a = ecalloc(1, size);
  *a = (arena){.size = size, .cursor = 0};
  a->tail = a;
  return a;
}

arena *mk_arena(void) {
  return mk_arena_sized(PAGESIZE);
}

void destroy_arena(arena *a) {
  if (a) {
    destroy_arena(a->next);
    free(a);
  }
}

void *arena_alloc(arena *head, size_t nmemb, size_t mem_size) {
  arena *tail = head->tail;

  size_t size, required;

  size = nmemb * mem_size;
  required = tail->cursor + size + offsetof(arena, buffer);

  if (required > tail->size) {
    size_t sz = size + offsetof(arena, buffer);
    // Create new arena aligned to the next pagesize boundary
    size_t new_arena_size = sz - (sz % __pagesize) + __pagesize;
    arena *new_tail = mk_arena_sized(new_arena_size);
    head->tail = tail->tail = new_tail;
    tail->next = new_tail;

    tail = new_tail;
    required = tail->cursor + size + offsetof(arena, buffer);
  }

  void *ret = tail->buffer + tail->cursor;
  tail->cursor += size;
  return ret;
}
