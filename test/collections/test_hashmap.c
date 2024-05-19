// link collections/hashmap.o collections.o logging.o
#include <stdlib.h>
#include <string.h>

#include "collections/hashmap.h"
#include "logging.h"

#define assert(X) if (!(X)) { die("Assertion failed: %s", #X); }

#define mk_kvp(_k, _v) \
  (hashmap_kvp) { .key = mk_slice(_k), .value = &_v }

void simple_hashmap_test(void) {
  hashmap_t h = mk_hashmap();
  int value = 1;
  assert(hashmap_lookup(&h, mk_slice("hello")) == NULL);
  assert(hashmap_add(&h, mk_kvp("hello", value)));
  assert(!hashmap_add(&h, mk_kvp("hello", value)));
  assert(hashmap_set(&h, mk_kvp("hello", value)) == &value);
  int *value_ptr = hashmap_lookup(&h, mk_slice("hello"));
  assert(*value_ptr == 1);
  value = 2;
  assert(*value_ptr == 2);
  assert(hashmap_remove(&h, mk_slice("hello")) == &value);
  assert(hashmap_remove(&h, mk_slice("hello")) == NULL);
  destroy_hashmap(&h);
}

void bigger_hashmap_test(void) {
  hashmap_t h = mk_hashmap();
  for (int i = 0; i < 100; i++) {
    char *key = ecalloc(1, sizeof(char));
    *key = i;
    int *value = ecalloc(1, sizeof(int));
    *value = i;
    hashmap_kvp kvp = {
        .value = value,
        .key = (string_slice){.n = 1, .str = key},
    };
    assert(hashmap_add(&h, kvp));
    assert(!hashmap_add(&h, kvp));
    assert(hashmap_set(&h, kvp) == value);
    assert(hashmap_remove(&h, kvp.key) == value);
    assert(hashmap_add(&h, kvp));
    atexit_r(free, key);
    atexit_r(free, value);
  }

  for (int i = 0; i < 100; i++) {
    string_slice key = {.n = 1, .str = &(char){i}};
    int *value = hashmap_lookup(&h, key);
    assert(*value == i);
    assert(value == hashmap_remove(&h, key));
    assert(NULL == hashmap_remove(&h, key));
    hashmap_kvp kvp = {.value = value, .key = key};
    assert(hashmap_add(&h, kvp));
  }

  destroy_hashmap(&h);
}

int main(void) {
  simple_hashmap_test();
  bigger_hashmap_test();
  return 0;
}
