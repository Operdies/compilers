#include <errno.h>
#include <execinfo.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "macros.h"

#define WARN_COLOR "\033[1;33m"
#define ERROR_COLOR "\033[1;31m"
#define FATAL_COLOR "\033[1;31m"
#define INFO_COLOR "\033[1;34m"
#define DEBUG_COLOR "\033[1;36m"
#define RESET_COLOR "\033[0m"

enum loglevel loglevel = DEBUG;

static const char *log_colors[] = {
    [DEBUG] = DEBUG_COLOR,
    [INFO] = INFO_COLOR,
    [WARN] = WARN_COLOR,
    [ERROR] = ERROR_COLOR,
    [FATAL] = FATAL_COLOR,
};

static const char *headers[] = {
    [DEBUG] = "DEBUG",
    [INFO] = "INFO",
    [WARN] = "WARN",
    [ERROR] = "ERROR",
    [FATAL] = "FATAL",
};

static void
colored_log(FILE *fp, enum loglevel level, const char *fmt, va_list ap) {
  if (loglevel >= level) {
    const char *color = log_colors[level];
    const char *header = headers[level];
    fprintf(fp, "%s[ %-*s ] ", color, 5, header);
    if (ap)
      vfprintf(fp, fmt, ap);
    else
      fputs(fmt, fp);
    fprintf(fp, RESET_COLOR);
    fputs("\n", fp);
  }
}

void set_loglevel(enum loglevel level) {
  loglevel = level;
}

void debug(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  colored_log(stdout, DEBUG, fmt, ap);
  va_end(ap);
}

void info(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  colored_log(stdout, INFO, fmt, ap);
  va_end(ap);
}

void warn(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  colored_log(stderr, WARN, fmt, ap);
  va_end(ap);
}

void error(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  colored_log(stderr, ERROR, fmt, ap);
  va_end(ap);
}

void die(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  colored_log(stderr, FATAL, fmt, ap);
  va_end(ap);

  if (fmt[0] && fmt[strlen(fmt) - 1] == ':' && errno != 0) {
    colored_log(stderr, FATAL, strerror(errno), NULL);
  }

  { // print stacktrace
    void *array[10];
    char **strings;
    int size, i;

    size = backtrace(array, LENGTH(array));
    strings = backtrace_symbols(array, size);
    for (i = 0; i < size; i++) {
      char *str = strings[i];
      str = strrchr(str, '/') + 1;
      debug(str);
    }
  }

  exit(1);
}

static void handler(int sig) {
  const char *signal = strsignal(sig);
  error(signal);
  { // print stacktrace
    void *array[10];
    char **strings;
    int size, i;

    size = backtrace(array, LENGTH(array));
    strings = backtrace_symbols(array, size);
    // skip 2 entries:
    // 0: handler (this)
    // 1: signal handler from libc
    for (i = 2; i < size; i++) {
      char *str = strings[i];
      str = strrchr(str, '/') + 1;
      debug(str);
    }
  }
  exit(sig);
}

void setup_crash_stacktrace_logger(void) {
  signal(SIGSEGV, handler);
  signal(SIGINT, handler);
}
