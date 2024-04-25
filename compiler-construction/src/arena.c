#include "arena.h"
#include "logging.h"
#include <sys/mman.h>
#include <unistd.h>

static long __pagesize = 0;
#define PAGESIZE (__pagesize ? __pagesize : (__pagesize = sysconf(_SC_PAGESIZE)))

static arena *mk_arena_sized(size_t size) {
  arena *a;

  // acquire virtual addressing for ARENA_COMMIT bytes (possibly more than the system has available)
  // The upper limit is system dependent.
  a = mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (a == MAP_FAILED)
    die("mmap:");

  // Allocate a single page to store information about the arena
  if (mprotect(a, PAGESIZE, PROT_READ | PROT_WRITE))
    die("mprotect:");

  *a = (arena){.size = size, .cursor = 0, .committed = PAGESIZE};
  a->tail = a;
  return a;
}

arena *mk_arena(void) {
  return mk_arena_sized(PAGESIZE);
}

void destroy_arena(arena *a) {
  if (a) {
    destroy_arena(a->next);
    if (munmap(a, a->size))
      die("munmap:");
  }
}

void *arena_alloc(arena *head, size_t nmemb, size_t mem_size) {
  arena *tail = head->tail;

  size_t size, required, to_commit;
  void *ret = tail->buffer + tail->cursor;

  size = nmemb * mem_size;
  required = tail->cursor + size + offsetof(arena, buffer);

  if (required > tail->size) {
    size_t sz = size + offsetof(arena, buffer);
    // Create new arena which can fit at least the allocation.
    size_t new_arena_size = sz - (sz % __pagesize) + __pagesize;
    arena *new_tail = mk_arena_sized(new_arena_size);
    head->tail = new_tail;
    tail->next = new_tail;
    tail = new_tail;

    required = tail->cursor + size + offsetof(arena, buffer);
  }

  if (required > tail->committed) {
    // mprotect must be page aligned -- claim as many whole pages as needed
    to_commit = required - (required % __pagesize) + __pagesize;
    if (mprotect(tail, to_commit, PROT_READ | PROT_WRITE))
      die("mprotect:");

    { // touch each newly allocated page immediately. This forces them to be physically allocated.
      // On real hardware, this is typically not needed, but when virtualization comes into the mix,
      // we risk a SIGBUS when the memory is dereferenced. It is better in this case to fail early
      char *loc = (char *)tail;
      for (size_t page_start = tail->committed; page_start < +to_commit; page_start += __pagesize)
        loc[page_start] = 0;
    }

    tail->committed = to_commit;
  }

  tail->cursor += size;
  return ret;
}
