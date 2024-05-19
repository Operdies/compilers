#include "collections/hashmap.h"

#include <stdlib.h>
#include <string.h>

#include "collections.h"

static hashmap_t mk_hashmap_cap(int initial_capacity);
static void destroy_kvp_list(kvp_lst *v);
static void destroy_kvp(kvp_lst *v);

static int _hash(const hashmap_key key, int cap) {
  const int F = 53;
  const int A = 86969;
  const int B = 76963;
  size_t v = F;
  for (int i = 0; i < key.n; i++) {
    v = (v * A) ^ (key.str[i] * B);
  }
  return v % cap;
}

static int key_index(const hashmap_t *m, const hashmap_key key) {
  int h = _hash(key, m->c);
  return h;
}

static float load_factor(const hashmap_t *m) { return (float)m->n / (float)m->c; }

// Assume n is odd and positive
static bool is_prime(int n) {
  for (int i = 3; i * i <= n; i += 2) {
    if (n % i == 0)
      return false;
  }
  return true;
}
static int next_capacity(int current) {
  for (current = current * 2 + 1; !is_prime(current); current += 2)
    ;
  return current;
}

static void resize_if_loaded(hashmap_t *m) {
  if (load_factor(m) > 0.70f) {
    hashmap_t new = mk_hashmap_cap(next_capacity(m->c));
    for (int i = 0; i < m->c; i++) {
      kvp_lst *kvp = m->values[i];
      while (kvp) {
        hashmap_kvp k = {.key = kvp->k, .value = kvp->v};
        hashmap_add(&new, k);
        kvp_lst *n = kvp->next;
        destroy_kvp(kvp);
        kvp = n;
      }
    }
    free(m->values);
    m->values = new.values;
    m->c = new.c;
  }
}

// Return the value of the given key if it exists in the map
hashmap_value hashmap_lookup(const hashmap_t *m, const hashmap_key key) {
  if (m->n) {
    int h = key_index(m, key);
    for (kvp_lst *v = m->values[h]; v; v = v->next) {
      if (slicecmp(v->k, key) == 0)
        return v->v;
    }
  }
  return NULL;
}

static string_slice clone_slice(string_slice s) {
  // We need to copy the key. If the key is modified externally, internal invariants will break.
  char *copy = ecalloc(s.n, sizeof(char));
  memcpy(copy, s.str, s.n);
  return (string_slice){.n = s.n, .str = copy};
}

// Add the given kvp to the map if it does not exist. Return true if a new value was inserted.
bool hashmap_add(hashmap_t *m, hashmap_kvp kvp) {
  if (kvp.key.n == 0)
    return false;
  int h = key_index(m, kvp.key);
  kvp_lst *lst = m->values[h];
  for (kvp_lst *v = lst; v; v = v->next) {
    if (slicecmp(v->k, kvp.key) == 0)
      return false;
  }

  kvp_lst *new = ecalloc(1, sizeof(kvp_lst));
  new->k = clone_slice(kvp.key);
  new->v = kvp.value;
  new->next = m->values[h];
  m->values[h] = new;
  m->n++;
  resize_if_loaded(m);
  return true;
}
// Set the given kvp in the map. If it already existed, return the removed value.
hashmap_value hashmap_set(hashmap_t *m, hashmap_kvp kvp) {
  if (kvp.key.n == 0 || m->n == 0)
    return NULL;

  int h = key_index(m, kvp.key);
  for (kvp_lst *v = m->values[h]; v; v = v->next) {
    if (slicecmp(v->k, kvp.key) == 0) {
      hashmap_value res = v->v;
      v->v = kvp.value;
      return res;
    }
  }

  kvp_lst *new = ecalloc(1, sizeof(kvp_lst));
  new->k = clone_slice(kvp.key);
  new->v = kvp.value;
  new->next = m->values[h];
  m->values[h] = new;
  m->n++;
  resize_if_loaded(m);
  return NULL;
}

// Remove a kvp from the map. Return the removed value.
hashmap_value hashmap_remove(hashmap_t *m, const hashmap_key key) {
  int h;
  kvp_lst *v, *p;

  v = p = NULL;
  h = key_index(m, key);
  if ((v = m->values[h])) {
    for (; v; p = v, v = v->next) {
      if (slicecmp(key, v->k) == 0) {
        if (p) {
          p->next = v->next;
        } else {
          m->values[h] = v->next;
        }
        break;
      }
    }
    kvp_lst *next = m->values[h];
    (void)next;
    if (v) {
      m->n--;
      hashmap_value val = v->v;
      destroy_kvp(v);
      return val;
    }
  }
  return NULL;
}

hashmap_t mk_hashmap_cap(int initial_capacity) {
  hashmap_t v = {0};
  v.c = initial_capacity;
  v.values = ecalloc(initial_capacity, sizeof(hashmap_value));
  return v;
}
// Create a new hashmap. It must be destroyed with destroy_hashmap
hashmap_t mk_hashmap(void) { return mk_hashmap_cap(3); }
// Destroy a hashmap

static void destroy_kvp(kvp_lst *v) {
  free((char *)v->k.str);
  free(v);
}
static void destroy_kvp_list(kvp_lst *v) {
  while (v) {
    kvp_lst *n = v->next;
    destroy_kvp(v);
    v = n;
  }
}
void destroy_hashmap(hashmap_t *m) {
  for (int i = 0; i < m->c; i++) {
    destroy_kvp_list(m->values[i]);
  }
  free(m->values);
}
