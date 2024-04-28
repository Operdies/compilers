// link arena.o logging.o collections.o
#include <unistd.h>

#include "arena.h"
#include "logging.h"
#include "unittest.h"

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
  char *middle = NULL;
  // Allocate some new crap
  int sz = to_allocate / steps;
  for (int i = 0; i < steps; i++) {
    char *arr = arena_alloc(a, sz, 1);

    if (i == steps / 2) {
      middle = arr;
      for (int j = 0; j < sz; j++) {
        arr[j] = (char)(j % 128);
      }
    }
  }

  assert2(middle);

  for (int i = 0; i < sz; i++) {
    assert2(middle[i] == i % 128);
  }
  // Verify the original allocation
  for (int i = 0; i < initial_alloc; i++) {
    assert2(fst[i] == i % 128);
  }
  assert2(log_severity() <= INFO);
  destroy_arena(a);
  return 0;
}
