#ifndef __ARENA_H
#define __ARENA_H

#include <stddef.h>

struct arena;
typedef struct arena {
  size_t size;        // Number of mapped bytes.
  size_t cursor;      // Index to end of buffer.
  struct arena *next; // The next arena in a linked list.
  struct arena *tail; // The tail of the linked list of arenas. This is only guaranteed to be correct on the head of the list.
  char buffer[];      // the start of user allocated data
} arena;

// allocate a new arena
arena *mk_arena(void);
// Unmap the memory associated with an arena, including the arena itself
void destroy_arena(arena *a);
// allocate nmemb * sz bytes of memory
void *arena_alloc(arena *a, size_t nmemb, size_t size);
#endif // __ARENA_H
