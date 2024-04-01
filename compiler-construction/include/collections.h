#include <stdbool.h>

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

#define v_foreach(type, var, vec) \
  type var;                     \
  for (int idx_##var = 0; idx_##var < vec.n && (var = vec_nth(&vec.slice, idx_##var)); idx_##var++)

// generically sized vector
typedef struct {
  union {
    vslice slice;
    string_slice string;
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
} string;

typedef int (*comparer_t)(const void *a, const void *b);
int slicecmp(string_slice s1, string_slice s2);
bool mk_vec(vec *v, int elem_size, int initial_capacity);
void vec_destroy(vec *v);
bool vec_push(vec *v, void *elem);
vec vec_clone(const vec *v);
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

/* Initializes s on success.
 * On failure, do nothing
 */
bool mk_string(string *s, int initial_capacity);
/* Append data to string, resizing if needed
 */
bool push_char(string *s, char ch);
bool push_str(string *s, int n, const char data[static n]);
bool string_contains(const string *s, char ch);
/* Free all data associated with the string
 */
void destroy_string(string *s);
// Create a new string from an existing char array
string string_from_chars(const char *src, int n);
