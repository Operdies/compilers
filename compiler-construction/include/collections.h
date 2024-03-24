#include <stdbool.h>
#include <stddef.h>

typedef struct {
  size_t length;
  size_t capacity;
  char *data;
} string;

/* Initializes s on success.
 * On failure, do nothing
 */
bool mk_string(string *s, size_t initial_capacity);
/* Append data to string, resizing if needed
 */
bool push_str(string *s, size_t n, char data[static n]);
/* Free all data associated with the string
 */
void destroy_string(string *s);
