#include "collections.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "macros.h"

static void *identity(void *v) { return v; }

struct cleanup_list {
  void *arg;
  cleanup_func f;
  struct cleanup_list *next;
};

static struct cleanup_list *cleanup = NULL;
static void destroy_cleanup_list(void) {
  while (cleanup) {
    struct cleanup_list *next = cleanup->next;
    cleanup->f(cleanup->arg);
    free(cleanup);
    cleanup = next;
  }
}

void atexit_r(cleanup_func f, void *arg) {
  if (cleanup == NULL) {
    atexit(destroy_cleanup_list);
  }
  struct cleanup_list *entry = ecalloc(1, sizeof(struct cleanup_list));
  *entry = (struct cleanup_list){
      .arg = arg,
      .f = f,
      .next = cleanup,
  };
  cleanup = entry;
}

void mk_string(string_t *s, int initial_capacity) { mk_vec(&s->v, 1, initial_capacity); }

void destroy_string(string_t *s) { vec_destroy(&s->v); }

void push_str(string_t *s, int n, const char data[static n]) {
  if (s) {
    vslice vec = {.n = n, .sz = 1, .array = (char *)data};
    vec_push_slice(&s->v, &vec);
  }
}

void mk_vec(vec *v, int elem_size, int initial_capacity) {
  void *data = ecalloc(initial_capacity, elem_size);
  v->n = 0;
  v->c = initial_capacity;
  v->sz = elem_size;
  v->array = data;
}

void ensure_capacity(vec *v, int c) {
  if (v->sz == 0)
    die("realloacing vector with element size 0.");
  if (v->c >= c)
    return;
  if (v->c <= 0)
    v->c = 1;
  while (v->c < c) {
    v->c *= 2;
    if (v->c <= 0) {
      die("Extremely large vector: 0x%x", c);
      v->c = c;
      break;
    }
  }
  if (v->array) {
    v->array = erealloc(v->array, v->c, v->sz);
  } else {
    v->array = ecalloc(v->c, v->sz);
  }
}
void vec_ensure_capacity(vec *v, int c) { ensure_capacity(v, c); }

void vec_push(vec *v, const void *elem) {
  ensure_capacity(v, v->n + 1);
  char *addr = (char *)v->array;
  memmove(addr + v->n * v->sz, elem, v->sz);
  v->n++;
}

void vec_set(vec *v, int n, void *elem) {
  int offset = n * v->sz;
  memmove((char *)v->array + offset, elem, v->sz);
}

void vec_swap(vec *v, int first, int second) {
  if (first == second)
    return;
  unsigned char tmp[v->sz];
  void *fst = vec_nth(*v, first);
  void *snd = vec_nth(*v, second);
  memcpy(tmp, fst, v->sz);
  memcpy(fst, snd, v->sz);
  memcpy(snd, tmp, v->sz);
}

void vec_sort(vec *v, comparer_t cmp) { qsort(v->array, v->n, v->sz, cmp); }

void vec_reverse(vec *v) {
  for (int i = 0; i < v->n / 2; i++)
    vec_swap(v, i, v->n - i - 1);
}

void vec_insert(vec *v, int index, void *elem) {
  if (index < 0 || index > v->n)
    die("vec_insert index out of range.");
  ensure_capacity(v, v->n + 1);

  char *arr = (char *)v->array;
  int insert_at = index * v->sz;
  int moved_bytes = (v->n - index) * v->sz;
  // Shift everyting after the insertion point by one
  memmove(arr + insert_at + v->sz, arr + insert_at, moved_bytes);
  // Insert the new value
  memmove(arr + insert_at, elem, v->sz);
  v->n++;
}

void *vec_pop(vec *v) {
  if (v->n <= 0)
    return NULL;
  v->n -= 1;
  int offset = v->n * v->sz;
  return (char *)v->array + offset;
}

bool vec_push_slice(vec *destination, const vslice *source) {
  if (!destination || !source)
    return false;
  if (destination->sz != source->sz)
    return false;

  // we need to create a copy of source before re-allocating
  // because source and destinatin might overlap
  vec copy = vec_select(source, source->sz, identity);
  source = &copy.slice;
  ensure_capacity(destination, destination->n + source->n);

  char *dest = (char *)destination->array + destination->n * destination->sz;
  char *src = (char *)source->array;
  memmove(dest, src, source->n * source->sz);

  // Optimization: Copy non-overlapping memory regions with memcpy

  destination->n += source->n;
  vec_destroy(&copy);
  return true;
}

bool vec_push_array(vec *v, int n, const void *data) {
  return vec_push_slice(v, &(vslice){.n = n, .sz = v->sz, .array = (void *)data});
}

static int roll(int x, int n) {
  if (x < 0) {
    x = -x;
    x = n - x;
    x++;
  }
  return x;
}

vslice vec_slice(vec *v, int start, int end) {
  vslice no_slice = {0};
  if (v) {
    start = roll(start, v->n);
    end = roll(end, v->n);
    if (start < 0 || end < 0 || start > end)
      return no_slice;
    return (vslice){.sz = v->sz, .n = end - start, .array = (char *)v->array + start * v->sz};
  }
  return no_slice;
}

void vec_clear(vec *v) { v->n = 0; }
void vec_zero(vec *v) {
  assert(v->sz == 0);
  memset(v->array, 0, v->c * v->sz);
}

void vec_destroy(vec *v) {
  free(v->array);
  *v = (vec){0};
}

void vec_foreach(vslice *v, vec_fn f) {
  if (v) {
    for (int i = 0; i < v->n; i++) {
      void *elem = vec_nth(*v, i);
      f(elem);
    }
  }
}

vec vec_select(const vslice *v, int elem_size, vec_selector s) {
  vec result = {.sz = elem_size};
  if (v) {
    for (int i = 0; i < v->n; i++) {
      void *elem = vec_nth(*v, i);
      void *new_elem = s(elem);
      vec_push(&result, new_elem);
    }
  }
  return result;
}

int slicecmp(string_slice s1, string_slice s2) {
  char c1, c2;
  c1 = c2 = 0;
  for (int i = 0; i < s1.n || i < s2.n; i++) {
    c1 = i >= s1.n ? 0 : s1.str[i];
    c2 = i >= s2.n ? 0 : s2.str[i];

    if (c1 == 0 || c2 == 0 || c1 != c2)
      break;
  }
  return c2 - c1;
}

vec vec_clone(const vec *v) { return vec_select(&v->slice, v->sz, identity); }

string_t string_from_chars(const char *src, int n) {
  string_t s = {0};
  mk_string(&s, n);
  push_str(&s, n, src);
  return s;
}

void push_char(string_t *s, char ch) { vec_push(&s->v, &ch); }
bool string_contains(const string_t *s, char ch) {
  for (int i = 0; i < s->n; i++) {
    if (s->chars[i] == ch)
      return true;
  }
  return false;
}

bool vec_contains(const vec *v, const void *elem) { return vslice_contains(&v->slice, elem); }

bool vslice_contains(const vslice *v, const void *elem) {
  for (int i = 0; i < v->n; i++) {
    const void *item = vec_nth(*v, i);
    if (memcmp(item, elem, v->sz) == 0)
      return true;
  }
  return false;
}

void *ecalloc(size_t nmemb, size_t size) {
  void *p;

  if (!(p = calloc(nmemb, size)))
    die("calloc:");
  return p;
}

void *erealloc(void *array, size_t nmemb, size_t size) {
  void *p;

  if (!(p = realloc(array, nmemb * size)))
    die("realloc:");

  return p;
}

static void translate_fmt(const char *fmt, vec *v) {
  static char *mappings[] = {['S'] = ".*s"};
  int n_mappings = LENGTH(mappings);
  bool inhibit = false;
  for (const char *ch = fmt; *ch; ch++) {
    vec_push(v, ch);
    if (*ch == '%' && !inhibit) {
      inhibit = true;
      int fmt = (int)(*(ch + 1));
      if (fmt < n_mappings && mappings[fmt]) {
        ch++;
        for (char *m = mappings[fmt]; *m; m++)
          vec_push(v, m);
      }
    } else {
      inhibit = false;
    }
  }
  char end = 0;
  vec_push(v, &end);
}

int vec_vwrite(vec *v, const char *fmt, va_list ap) {
  static vec fmt_buf = {.sz = sizeof(char)};
  if (fmt_buf.array == NULL) {
    ensure_capacity(&fmt_buf, 100);
    atexit_r((cleanup_func)vec_destroy, &fmt_buf);
  }
  fmt_buf.n = 0;
  translate_fmt(fmt, &fmt_buf);
  const char *format = fmt_buf.array;
  // Unfortunate that this requires an extra allocation 
  char *temp;
  int n = vasprintf(&temp, format, ap);
  ensure_capacity(v, v->n + n);
  memcpy((char *)v->array + v->n * v->sz, temp, n);
  free(temp);
  v->n += n;
  return n;
}

int vec_write(vec *v, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int written = vec_vwrite(v, fmt, ap);
  va_end(ap);
  return written;
#undef wrt
}

void vec_fcopy(vec *v, FILE *f) {
  int read;
  char buf[4096];
  do {
    read = fread(buf, 1, sizeof(buf), f);
    vec_push_array(v, read, buf);
  } while (read == 4096);
}

char *string_slice_clone(string_slice s) { return strndup(s.str, s.n + 1); }
