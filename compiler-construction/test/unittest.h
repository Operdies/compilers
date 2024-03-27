#ifndef _UNITTEST_H
#define _UNITTEST_H
#include <stdio.h>
#include <stdlib.h>

int panic(const char *file, int line, const char *expr){
  fprintf(stderr, "%s:%d\nAssertion failed: %s\n", file, line, expr);
  exit(1);
  return 0;
}

#define ASSERT(X) if (! (X)) { panic(__FILE__, __LINE__, #X); }
#define ASSERT_EQ(X, Y) ASSERT(X == Y)
#define ASSERT_NEQ(X, Y) ASSERT((X) != (Y))
#define ASSERT_STREQ(X, Y) ASSERT(strcmp(X, Y) == 0)

#endif

