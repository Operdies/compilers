// link: logging.o collections.o
#include "logging.h"

// This is not so much a test as just some code that tries each color and exits

int main(void) {
  debug("Debug");
  info("Info");
  warn("Warning");
  error("Error");
  debug("thing\n  on multiple\nlines");
  return log_severity() == LL_DEBUG;
}
