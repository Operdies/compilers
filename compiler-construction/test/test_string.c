// link: collections.o
#include "collections.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  int i = 0;
  string s = {0};
  char *something = "something";
  mk_string(&s, 1);
  if (s.data == NULL) {
    return -1;
  }
  printf("String %d: (%ld) %.*s\n", ++i, s.length, (int)s.length, s.data);
  push_str(&s, strlen(something), something);
  printf("String %d: (%ld) %.*s\n", ++i, s.length, (int)s.length, s.data);
  for (int j = 0; j < 10; j++) {
    push_str(&s, s.length, s.data);
    // printf("String %d: (%ld) %.*s\n", ++i, s.length, (int)s.length, s.data);
  }
  printf("Final capacity: %ld kB\n", s.length / 1000);
  destroy_string(&s);

  return 0;
}
