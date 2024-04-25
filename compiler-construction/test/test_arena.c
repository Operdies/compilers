// link arena.o logging.o collections.o
#include "arena.h"
#include "logging.h"
#include "unittest.h"
#include <unistd.h>

int main(void) {
  arena *a = mk_arena();
  int initial_alloc = 2000;
  char *fst = arena_alloc(a, initial_alloc, 1);

  // Allocate some stuff
  for (int i = 0; i < initial_alloc; i++) {
    fst[i] = i % 128;
  }
  // Verify the values
  for (int i = 0; i < initial_alloc; i++) {
    assert2(fst[i] == i % 128);
  }

  size_t to_allocate = 1l << 16;
  int steps = 500;
  // Allocate some new crap
  for (int i = 0; i < steps; i++) {
    arena_alloc(a, to_allocate / steps, 1);
  }
  // Verify the original allocation
  for (int i = 0; i < initial_alloc; i++) {
    assert2(fst[i] == i % 128);
  }
  assert2(log_severity() <= INFO);
  destroy_arena(a);
  return 0;
}
