#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TEST_SIZE 512
static int work(int n, int arr[static n]) {
  srand(time(NULL));
  for (int i = 0; i < n; i++) {
    arr[i] = rand();
  }
  volatile int r = 0;
  for (volatile int i = 0; i < n; i++) {
    for (volatile int j = 0; j < n; j++) {
      for (volatile int k = 0; k < n; k++) {
        r += arr[(i * j * k) % n];
      }
    }
  }
  return r;
}

static void stack_test(int c) {
  (void)c;
  int arr[TEST_SIZE] = {0};
  work(TEST_SIZE, arr);
}

static void vstack_test(int c) {
  int arr[c];
  memset(arr, 0, c * sizeof(int));
  work(c, arr);
}
static void heap_test(int c) {
  int *arr = calloc(c, sizeof(int));
  work(c, arr);
  free(arr);
}

int main(int argc, char **argv) {
  (void)argc;
  int c = TEST_SIZE;
  void (*method)(int) = &stack_test;
  if (argc > 1) {
    if (strcmp("heap", argv[1]) == 0) {
      method = &heap_test;
    } else if (strcmp("vstack", argv[1]) == 0) {
      method = &vstack_test;
    }
  }
  method(c);
  return 0;
}

// Purpose: Confirming my assumption that the heap is just as fast as the stack as long as locality is good
// $ hyperfine --setup 'gcc -O2 heaper.c -o heaper' --parameter-list mem vstack,heap,stack './heaper {mem}'
// Benchmark 1: ./heaper vstack
//   Time (mean ± σ):      1.424 s ±  0.007 s    [User: 1.423 s, System: 0.001 s]
//   Range (min … max):    1.412 s …  1.438 s    10 runs
//
// Benchmark 2: ./heaper heap
//   Time (mean ± σ):      1.429 s ±  0.011 s    [User: 1.427 s, System: 0.002 s]
//   Range (min … max):    1.401 s …  1.439 s    10 runs
//
// Benchmark 3: ./heaper stack
//   Time (mean ± σ):      1.428 s ±  0.005 s    [User: 1.428 s, System: 0.000 s]
//   Range (min … max):    1.423 s …  1.437 s    10 runs
//
// Summary
//   ./heaper vstack ran
//     1.00 ± 0.01 times faster than ./heaper stack
//     1.00 ± 0.01 times faster than ./heaper heap
