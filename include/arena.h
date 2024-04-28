#ifndef __ARENA_H
#define __ARENA_H

#include <stddef.h>

struct arena;
typedef struct arena {
  size_t size;         // Number of reserved bytes.
  size_t cursor;       // Index to unallocated data.
  struct arena *next;  // Next element in a linked list.
  struct arena *tail;  // Tail of a linked list.
  char buffer[];       // Start of allocated data.
} arena;

// Create a new arena
arena *mk_arena(void);
// Free all memory associated with an arena
void destroy_arena(arena *a);
// Allocate nmemb * size bytes of memory
void *arena_alloc(arena *a, size_t nmemb, size_t size);
#endif  // __ARENA_H
