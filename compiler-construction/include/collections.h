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

// A view into a vector
typedef struct {
  // number of elements
  int n;
  // size of each element
  int sz;
  // array
  void *arr;
} vslice;

// generically sized vector
typedef struct {
  union {
    vslice slice;
    struct {
      // number of elements
      int n;
      // size of each element
      int sz;
      // array
      void *arr;
    };
  };
  // capacity
  int c;
} vec;

typedef int (*comparer_t)(const void *a, const void *b);
bool mk_vec(vec *v, int elem_size, int initial_capacity);
bool vec_push(vec *v, void *elem);
vec *vec_clone(const vec *v);
void vec_clear(vec *v);
void vec_sort(vec *v, comparer_t comp_fn);
void vec_reverse(vec *v);
void *vec_nth(const vslice *v, int n);
bool vec_push_slice(vec *v, const vslice *s);
vslice vec_slice(vec *v, int start, int end);

