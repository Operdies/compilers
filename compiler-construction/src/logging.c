#include <errno.h>
#include <execinfo.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "collections.h"
#include "logging.h"
#include "macros.h"

#define RESET_COLOR "\033[0m"

enum loglevel loglevel = DEBUG;

static const char *log_headers[] = {
    [WARN] = "\033[1;30;43m",
    [DEBUG] = "\033[1;30;46m",
    [INFO] = "\033[1;30;44m",
    [ERROR] = "\033[1;30;41m",
    [FATAL] = "\033[1;30;41m",
};

static const char *log_colors[] = {
    [DEBUG] = "\033[1;36m",
    [INFO] = "\033[1;34m",
    [WARN] = "\033[1;33m",
    [ERROR] = "\033[1;31m",
    [FATAL] = "\033[1;31m",
};

static const char *headers[] = {
    [DEBUG] = "DEBUG",
    [INFO] = "INFO",
    [WARN] = "WARN",
    [ERROR] = "ERROR",
    [FATAL] = "FATAL",
};

static FILE *log = NULL;
void set_loglevel(enum loglevel level) {
  loglevel = level;
}

bool set_log_location(const char *filename) {
  if (log)
    fclose(log);
  log = fopen(filename, "w");
  return log != NULL;
}

static bool add_colors(FILE *fp) {
  // NOTE: it would be cleaner to use isatty() here, but this call returns
  // false in the pseudoterminal I use for develpment.
  return fp->_fileno == stdout->_fileno || fp->_fileno == stderr->_fileno;
}
static bool should_log(FILE *fp, enum loglevel level) {
  if (fp->_fileno == stdout->_fileno || fp->_fileno == stderr->_fileno)
    return loglevel >= level;
  return true;
}

void colored_log(FILE *fp, enum loglevel level, const char *fmt, va_list ap) {
  static vec strbuf = {.sz = sizeof(char)};
  strbuf.n = 0;
  setup_crash_stacktrace_logger();
  if (!fp)
    return;
  if (log && fp != log)
    colored_log(log, level, fmt, ap);

  if (should_log(fp, level)) {
    const char *color = log_colors[level];
    const char *header = headers[level];

    if (ap)
      vec_vwrite(&strbuf, fmt, ap);
    else
      vec_write(&strbuf, fmt);

    vec_push(&strbuf, "\0");

    char *line = strtok(strbuf.array, "\n");
    for (; line; line = strtok(NULL, "\n")) {
      if (add_colors(fp))
        fprintf(fp, "%s %-*s %s %s", log_headers[level], 5, header, RESET_COLOR, color);
      else
        fprintf(fp, "[%-*s] ", 5, header);

      fprintf(fp, "%s\n", line);
    }

    if (add_colors(fp))
      fprintf(fp, RESET_COLOR);

    fflush(fp);
  }
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

static void print_stacktrace_friendly(void **array, int size, int skip) {
  const int depth = 5;
  char **strings;
  int top, bottom;
  // skip_end: _start, __libc_start_main, mangled libc symbol
  int skip_end = 3;

  strings = backtrace_symbols(array, size);
  for (top = skip; top < size && top < (depth + skip); top++) {
    char *str = strings[top];
    str = strrchr(str, '/') + 1;
    debug("%2d) %s", top - skip, str);
  }

  bottom = size - depth - skip_end;
  if (bottom < top) {
    bottom = top;
  } else if (bottom - top == 1) {
    // If exactly one element would be truncated, just print it instead of truncating
    bottom -= 1;
  } else {
    debug("    -----");
  }

  for (; bottom < size - skip_end; bottom++) {
    char *str = strings[bottom];
    str = strrchr(str, '/') + 1;
    debug("%2d) %s", bottom - skip, str);
  }
}
void die(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  colored_log(stderr, FATAL, fmt, ap);
  va_end(ap);

  if (fmt[0] && fmt[strlen(fmt) - 1] == ':' && errno != 0) {
    colored_log(stderr, FATAL, strerror(errno), NULL);
  }

  void *array[100];
  int size = backtrace(array, LENGTH(array));
  print_stacktrace_friendly(array, size, 0);

  if (log)
    fclose(log);
  exit(1);
}

static void handler(int sig) {
  const char *signal = strsignal(sig);
  error(signal);

  void *array[100];
  int size = backtrace(array, LENGTH(array));
  print_stacktrace_friendly(array, size, 2);

  if (log)
    fclose(log);
  exit(sig);
}

static bool init = false;
void setup_crash_stacktrace_logger(void) {
  if (!init) {
    init = true;
    int signals[] = {SIGINT, SIGSEGV, SIGTERM, SIGHUP, SIGQUIT};
    for (int i = 0; i < LENGTH(signals); i++)
      sigaction(signals[i], &(struct sigaction){.sa_handler = handler}, NULL);
  }
}
