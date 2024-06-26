#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#include <stdbool.h>
#include <stdio.h>

typedef struct {
  int n;
  const char *str;
} string_slice;

#define mk_slice(string) \
  (string_slice) { .str = string, .n = strlen(string) }

// A view into a vector
typedef struct {
  // number of elements
  int n;
  // array
  void *array;
  // size of each element
  int sz;
} vslice;

#define vec_nth(vec, n) (void *)((char *)(vec).array + ((n) * (vec).sz))

#define v_foreach(type, var, vec) \
  type *var;                      \
  for (int idx_##var = 0; idx_##var < vec.n && (var = vec_nth(vec, idx_##var)); idx_##var++)

#define v_rforeach(type, var, vec) \
  type *var;                       \
  for (int idx_##var = vec.n - 1; idx_##var >= 0 && (var = vec_nth(vec, idx_##var)); idx_##var--)

#define v_make(type) \
  (vec) { .sz = sizeof(type) }

// generically sized vector
typedef struct {
  union {
    vslice slice;
    struct {
      // number of elements
      int n;
      // array
      void *array;
      // size of each element
      int sz;
    };
  };
  // capacity
  int c;
} vec;

typedef struct {
  union {
    vec v;
    struct {
      int n;
      char *chars;
      int c;
    };
  };
} string_t;

typedef void (*cleanup_func)(void *data);
void atexit_r(cleanup_func f, void *arg);
typedef int (*comparer_t)(const void *a, const void *b);
int slicecmp(string_slice s1, string_slice s2);
void mk_vec(vec *v, int elem_size, int initial_capacity);
void vec_destroy(vec *v);
// Push an element to the array and return a pointer to the new element
void *vec_push(vec *v, const void *elem);
bool vec_push_array(vec *destination, int n, const void *data);
void vec_set(vec *v, int n, void *elem);
void vec_swap(vec *v, int first, int second);
void vec_reverse(vec *v);
void vec_insert(vec *v, int index, void *elem);
void *vec_pop(vec *v);
vec vec_clone(const vec *v);
void vec_clear(vec *v);
void vec_zero(vec *v);
void vec_sort(vec *v, comparer_t cmp);
void vec_fcopy(vec *v, FILE *f);
void vec_reverse(vec *v);
void vec_ensure_capacity(vec *v, int c);
bool vslice_contains(const vslice *v, const void *elem);
bool vec_contains(const vec *v, const void *elem);
bool vec_push_slice(vec *v, const vslice *s);
vslice vec_slice(vec *v, int start, int end);
typedef void (*vec_fn)(void *v);
typedef void *(*vec_selector)(void *v);
// Perform an operation on each element in a slice
void vec_foreach(vslice *v, vec_fn f);
/* Create a new vector of the same size as the input slice,
 * populated with elements returned from the selector function.
 */
vec vec_select(const vslice *v, int elem_size, vec_selector s);

/* Initializes s on success.
 * On failure, do nothing
 */
void mk_string(string_t *s, int initial_capacity);
/* Append data to string, resizing if needed
 */
void push_char(string_t *s, char ch);
void push_str(string_t *s, int n, const char data[static n]);
bool string_contains(const string_t *s, char ch);
/* Free all data associated with the string
 */
void destroy_string(string_t *s);
// Create a new string from an existing char array
string_t string_from_chars(const char *src, int n);

void *ecalloc(size_t nmemb, size_t size);
void *erealloc(void *array, size_t nmemb, size_t size);
int vec_vwrite(vec *v, const char *fmt, va_list ap);
int vec_write(vec *v, const char *fmt, ...);
char *string_slice_clone(string_slice s);

#endif
