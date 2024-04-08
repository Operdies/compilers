#include <errno.h>
#include <execinfo.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  setup_crash_stacktrace_logger();
  if (!fp)
    return;
  if (log && fp != log)
    colored_log(log, level, fmt, ap);

  if (should_log(fp, level)) {
    const char *color = log_colors[level];
    const char *header = headers[level];

    if (add_colors(fp))
      fprintf(fp, "%s %-*s %s %s", log_headers[level], 5, header, RESET_COLOR, color);
    else
      fprintf(fp, "[%-*s] ", 5, header);

    if (ap)
      vfprintf(fp, fmt, ap);
    else
      fputs(fmt, fp);
    if (add_colors(fp))
      fprintf(fp, RESET_COLOR);

    fputs("\n", fp);
    // flush to ensure correct ordering between stdout and stderr to avoid confusion
    // also probably a good idea to ensure files are written to disk before program exit
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

  if (log)
    fclose(log);
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
