#include "arena.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

static long __pagesize = 0;
#define PAGESIZE (__pagesize ? __pagesize : (__pagesize = sysconf(_SC_PAGESIZE)))


arena* mk_arena(void) {
  arena *a;

  // acquire virtual addressing for ARENA_COMMIT bytes (possibly more than the system has available)
  // The upper limit is system dependent.
  a = mmap(0, ARENA_COMMIT, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  if (a == MAP_FAILED)
    return NULL;

  // Allocate a single page to store information about the arena
  if (mprotect(a, PAGESIZE, PROT_READ|PROT_WRITE))
    return NULL;

  *a = (arena) { .size = ARENA_COMMIT, .cursor = 0, .committed = PAGESIZE };
  return a;
}

void destroy_arena(arena *a) {
  if (munmap(a, a->size)) {
    perror("munmap:");
    exit(1);
  }
}

void* arena_alloc(arena *a, size_t nmemb, size_t mem_size) {
  size_t size, required, to_commit;
  void *ret = a->buffer + a->cursor;

  size = nmemb * mem_size;
  required = a->cursor + size + offsetof(arena, buffer);

  if (required > a->size) {
    return NULL;
  }

  if (required > a->committed) {
    // mprotect must be page aligned -- claim as many whole pages as needed
    to_commit = required - (required % __pagesize) + __pagesize;
    if (mprotect(a, to_commit, PROT_READ|PROT_WRITE))
      return NULL;

    { // touch each newly allocated page immediately. This forces them to be physically allocated.
      // On real hardware, this is typically not needed, but when virtualization comes into the mix,
      // we risk a SIGBUS when the memory is dereferenced. It is better in this case to fail early
      char *loc = (char*)a;
      for (size_t page_start = a->committed; page_start < + to_commit; page_start += __pagesize)
        loc[page_start] = 0;
    }

    a->committed = to_commit;
  }

  a->cursor += size;
  return ret;
}

