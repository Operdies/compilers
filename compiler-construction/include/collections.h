#include <stdbool.h>

typedef struct {
  int length;
  int capacity;
  char *data;
} string;

/* Initializes s on success.
 * On failure, do nothing
 */
bool mk_string(string *s, int initial_capacity);
/* Append data to string, resizing if needed
 */
bool push_str(string *s, int n, char data[static n]);
/* Free all data associated with the string
 */
void destroy_string(string *s);

typedef struct {
  int n;
  const char *str;
} string_slice;

// A view into a vector
typedef struct {
  // number of elements
  int n;
  // array
  void *arr;
  // size of each element
  int sz;
} vslice;

// generically sized vector
typedef struct {
  union {
    vslice slice;
    string_slice string;
    struct {
      // number of elements
      int n;
      // array
      void *arr;
      // size of each element
      int sz;
    };
  };
  // capacity
  int c;
} vec;

typedef int (*comparer_t)(const void *a, const void *b);
bool mk_vec(vec *v, int elem_size, int initial_capacity);
void vec_destroy(vec *v);
bool vec_push(vec *v, void *elem);
vec *vec_clone(const vec *v);
void vec_clear(vec *v);
void vec_sort(vec *v, comparer_t comp_fn);
void vec_reverse(vec *v);
void *vec_nth(const vslice *v, int n);
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
