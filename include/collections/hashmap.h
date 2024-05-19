#pragma once

#include "collections.h"

typedef string_slice hashmap_key;
typedef int *hashmap_value;

typedef struct kvp_lst kvp_lst;
struct kvp_lst {
  hashmap_key k;
  hashmap_value v;
  kvp_lst *next;
};
typedef struct {
  // number of elements in table
  int n;
  // capacity of collections
  int c;
  // array of linked list of kvps
  kvp_lst **values;
} hashmap_t;

typedef struct {
  hashmap_key key;
  hashmap_value value;
} hashmap_kvp;

// Return the value of the given key if it exists in the map
hashmap_value hashmap_lookup(const hashmap_t *, const hashmap_key key);
// Add the given kvp to the map if it does not exist. Return true if a new value was inserted.
bool hashmap_add(hashmap_t *, hashmap_kvp kvp);
// Set the given kvp in the map. If it already existed, return the removed value.
hashmap_value hashmap_set(hashmap_t *, hashmap_kvp kvp);
// Remove a kvp from the map. Return the removed value.
hashmap_value hashmap_remove(hashmap_t *, const hashmap_key key);
// Create a new hashmap. It must be destroyed with destroy_hashmap
hashmap_t mk_hashmap(void);
// Destroy a hashmap
void destroy_hashmap(hashmap_t *);
