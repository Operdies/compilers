#include "collections.h"

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
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

static struct cleanup_list *cleanup_head = NULL;
static void destroy_cleanup_list(void) {
  while (cleanup_head) {
    struct cleanup_list *next = cleanup_head->next;
    cleanup_head->f(cleanup_head->arg);
    free(cleanup_head);
    cleanup_head = next;
  }
}

void atexit_r(cleanup_func f, void *arg) {
  static struct cleanup_list *cleanup_tail = NULL;
  struct cleanup_list **insert_at = NULL;

  if (cleanup_tail) {
    insert_at = &cleanup_tail->next;
  } else {
    atexit(destroy_cleanup_list);
    insert_at = &cleanup_head;
  }

  struct cleanup_list *entry = ecalloc(1, sizeof(struct cleanup_list));
  *entry = (struct cleanup_list){
      .arg = arg,
      .f = f,
      .next = NULL,
  };
  *insert_at = cleanup_tail = entry;
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

static void vec_push_string(vec *v, char *str) {
  while (str && *str)
    vec_push(v, str++);
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

static void push_number(vec *v, uint64_t val, int base) {
  char lookup[] = "0123456789abcdef";
  int insert_at = v->n;
  do {
    int digit = val % base;
    vec_insert(v, insert_at, lookup + digit);
    val /= base;
  } while (val);
}

struct format_specifier {
  bool left;
  bool use_precision;
  int min;
  int precision;
  bool sign;
};

static const char *parseint(const char *ch, int *value) {
  int r = 0;
  while (*ch >= '0' && *ch <= '9') {
    r = r * 10 + (*ch - '0');
    ch++;
  }
  *value = r;
  return ch;
}

static const char *next_specifier(struct format_specifier *spec, const char *ch, va_list ap) {
  bool leftJustify = false;
  bool forceSign = false;
  bool usePrecision = false;
  int minWidth = 0;
  int maxWidth = 0;

  while (*ch == '-' || *ch == '+') {
    if (*ch == '-')
      leftJustify = true;
    else if (*ch == '+')
      forceSign = true;
    ch++;
  }

  if (*ch >= '0' && *ch <= '9') {
    ch = parseint(ch, &minWidth);
  } else if (*ch == '*') {
    minWidth = va_arg(ap, int);
    ch++;
  }

  if (*ch == '.') {
    usePrecision = true;
    ch++;
    if (*ch >= '0' && *ch <= '9') {
      ch = parseint(ch, &maxWidth);
    } else if (*ch == '*') {
      maxWidth = va_arg(ap, int);
      ch++;
    }
  }

  *spec = (struct format_specifier){
      .left = leftJustify,
      .min = minWidth,
      .precision = maxWidth,
      .sign = forceSign,
      .use_precision = usePrecision,
  };
  return ch;
}

// Calculate the number of digits a number has in a given base
static int n_digits(int n, int base) {
  if (n == 0)
    return 1;
  int result = 0;
  for (; n; n /= base, result++)
    ;
  return result;
}

/** Why does this exist?
 * I did not want to write %.*s string_slice.n, string_slice.str every time I need to log a string slice.
 * I realized it would be possible to exploit the memory layout of a string_slice struct and 'cheat' by specifying the
 * struct once like 'printf("%.*s", string_slice)'. This works (with gcc? in debug builds?), but I realized that it
 * does not work for printing e.g. vectors by specifying a format character for each struct member. I suspect it
 * has to do with struct padding, so I concluded it would not be a reliable solution, even if it works now.
 * Besides, a count mismatch in printf leads to compiler warnings.
 *
 * As an alternative solution, I looked into extending printf with custom format handlers, but it seems generally
 * non-portable and discouraged. So when I realized I would need to do manual format handling in the vec writer, I
 * thought "this opens up new possibilities", so of course I had to add a custom format argument for vectors. Why
 * shouldn't my vector writer be able to write nicely formatted vectors?
 *
 * To be honest, this has gotten out of hand.
 *
 */
int vec_vwrite(vec *v, const char *fmt, va_list ap) {
  int start = v->n;

  for (const char *ch = fmt; *ch; ch++) {
    if (*ch == '%') {
      if (*(ch + 1) == '%') {
        vec_push(v, ch);
        ch++;
        continue;
      }
      ch++;

      int specifier_start = v->n;
      struct format_specifier spec = {0};
      ch = next_specifier(&spec, ch, ap);

      bool is_numeric = false;

      switch (*ch) {
        case 'V': {
          vec v2 = va_arg(ap, vec);
          int digits = n_digits(v2.n - 1, 10);
#define vecprinter(type, fmt)                                                                \
  vec_push_string(v, "{\n");                                                                 \
  v_foreach(type, val, v2) {                                                                 \
    vec_write(v, "  [%d]%*s= " fmt, idx_val, 1 + digits - n_digits(idx_val, 10), " ", *val); \
    vec_push_string(v, ",\n");                                                               \
  }                                                                                          \
  vec_push_string(v, "\n}");                                                                 \
  ch++;

          switch (*(ch + 1)) {
            case 'd': {
              vecprinter(int, "%d");
            } break;
            case 'c': {
              vecprinter(char, "%c");
            } break;
            case 's': {
              vecprinter(char *, "%s");
            } break;
            default: {
              vec_write(v, "{ .n=%d, .sz=%d, .c=%d, .array=%p }", v2.n, v2.sz, v2.c, v2.array);
            } break;
          }
        } break;
        case 'd': {
          is_numeric = true;
          int val = va_arg(ap, int);
          spec.sign |= val < 0;
          if (spec.sign) {
            vec_push(v, val < 0 ? "-" : "+");
            if (val < 0)
              val = -val;
          }
          push_number(v, val, 10);
        } break;
        case 'c': {
          char val = va_arg(ap, int);
          vec_push(v, &val);
        } break;
        case 'n': {
          int *ptr = va_arg(ap, int *);
          *ptr = start - v->n;
        } break;
        case 'p': {
          uint64_t val = va_arg(ap, uint64_t);
          vec_push_string(v, "0x");
          push_number(v, val, 16);
        } break;
        case 's': {
          char *str = va_arg(ap, char *);
          int len = spec.use_precision ? strnlen(str, spec.precision) : strlen(str);
          vec_push_array(v, len, str);
        } break;
        case 'S': {
          string_slice str = va_arg(ap, string_slice);
          int n = str.n;
          if (spec.use_precision && spec.precision < str.n)
            n = spec.precision;
          vec_push_array(v, n, str.str);
        } break;
        default: {
          die("Unhandled format specifier: %c", *ch);
        }
      }

      int written = v->n - specifier_start;
      if (spec.use_precision && is_numeric) {
        for (int i = written; i < spec.precision + spec.sign; i++)
          vec_insert(v, spec.sign ? specifier_start + 1 : specifier_start, "0");
        written = v->n - specifier_start;
      }
      while (written < spec.min) {
        if (spec.left)
          vec_push(v, " ");
        else
          vec_insert(v, specifier_start, " ");
        written++;
      }
    } else {
      vec_push(v, ch);
    }
  }

  // Ensure the string is null terminated but don't include it in the length
  vec_push(v, &(char){0});
  v->n--;
  return v->n - start;
}

int vec_write(vec *v, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int written = vec_vwrite(v, fmt, ap);
  va_end(ap);
  return written;
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
