// link ebnf/ebnf.o ebnf/analysis.o scanner/scanner.o
// link regex.o arena.o collections.o logging.o
// link interpreters/lisp/test_lisp_compiler.o

#include "../unittest.h"
#include "logging.h"
#include "macros.h"
#include "interpreters/lisp/test_lisp_compiler.h"

static char test_program[] = {
#include "test.lisp"
};

int main(void) {
  set_loglevel(LL_DEBUG);
  setup_crash_stacktrace_logger();
  lisp_eval(test_program);
  assert2(log_severity() <= LL_INFO);
  return 0;
}
