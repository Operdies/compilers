#ifndef __ARENA_H
#define __ARENA_H

#ifndef ARENA_COMMIT
#define ARENA_COMMIT (1l<<30)
#endif

#include <stddef.h>

typedef struct {
  size_t size;      // Number of mapped bytes.
  size_t cursor;    // Index to end of buffer.
  size_t committed; // Number of mapped bytes that are allocated. Increased as needed in arena_alloc
  char buffer[];    // the start of user allocated data
} arena;

// allocate a new arena
arena* mk_arena(void);
// Unmap the memory associated with an arena, including the arena itself
void destroy_arena(arena *a);
// allocate nmemb * sz bytes of memory
void *arena_alloc(arena *a, size_t nmemb, size_t size);
#endif // __ARENA_H
