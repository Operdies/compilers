#include "arena.h"
#include "collections.h"
#include <stdlib.h>
#include <unistd.h>

// Arenas should be aligned to page boundaries
#define PAGESIZE 4096 // (PAGESIZE ? PAGESIZE : (PAGESIZE = sysconf(_SC_PAGESIZE)))

static arena *mk_arena_sized(size_t size) {
  // PERFORMANCE: I don't think calloc guarantees a page aligned memory region just because the region would align with a page boundary.
  // Use something like memalign instead.
  size += offsetof(arena, buffer);
  size = size - (size % PAGESIZE) + PAGESIZE;
  arena *a;
  a = ecalloc(1, size);
  *a = (arena){.size = size, .cursor = 0};
  a->tail = a;
  return a;
}

arena *mk_arena(void) {
  // NULL creates an arena with a single page.
  return mk_arena_sized(0);
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
    arena *new_tail = mk_arena_sized(size);
    head->tail = tail->tail = new_tail;
    tail->next = new_tail;

    tail = new_tail;
    required = tail->cursor + size + offsetof(arena, buffer);
  }

  void *ret = tail->buffer + tail->cursor;
  tail->cursor += size;
  return ret;
}
