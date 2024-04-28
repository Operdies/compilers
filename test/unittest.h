#ifndef _UNITTEST_H
#define _UNITTEST_H

#include <stdlib.h>

#include "logging.h"

void _assert_failure(const char *func, const char *file, int lineno, const char *expr);
void _assert_failure(const char *func, const char *file, int lineno, const char *expr) {
  error("%s %s:%d:\nAssertion `%s' failed.", func, file, lineno, expr);
  exit(1);
}

#define assert2(expr)                                     \
  if (expr)                                               \
    ;                                                     \
  else {                                                  \
    _assert_failure(__func__, __FILE__, __LINE__, #expr); \
  }

#endif
