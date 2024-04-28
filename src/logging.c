#include "logging.h"

#include <errno.h>
#include <execinfo.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "collections.h"
#include "macros.h"

#define RESET_COLOR "\033[0m"

enum loglevel loglevel = LL_DEBUG;
enum loglevel most_severe_log = LL_DEBUG;

static const char *log_headers[] = {[LL_WARN] = "\033[1;30;43m",
                                    [LL_DEBUG] = "\033[1;30;46m",
                                    [LL_INFO] = "\033[1;30;44m",
                                    [LL_ERROR] = "\033[1;30;41m",
                                    [LL_FATAL] = "\033[1;30;41m"};

static const char *log_colors[] = {[LL_DEBUG] = "\033[1;36m",
                                   [LL_INFO] = "\033[1;34m",
                                   [LL_WARN] = "\033[1;33m",
                                   [LL_ERROR] = "\033[1;31m",
                                   [LL_FATAL] = "\033[1;31m"};

static const char *headers[] = {
    [LL_DEBUG] = "DEBUG", [LL_INFO] = "INFO", [LL_WARN] = "WARN", [LL_ERROR] = "ERROR", [LL_FATAL] = "FATAL"};

static FILE *log = NULL;
enum loglevel set_loglevel(enum loglevel level) {
  int current = loglevel;
  loglevel = level;
  return current;
}
enum loglevel get_loglevel(void) { return loglevel; }
enum loglevel log_severity(void) { return most_severe_log; }

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
    return loglevel <= level;
  return true;
}

static vec strbuf = {.sz = sizeof(char)};
static void destroy_strbuf(void) { vec_destroy(&strbuf); }
void colored_log(FILE *fp, enum loglevel level, const char *fmt, va_list ap) {
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
      header = "  >";

      fprintf(fp, "%s\n", line);
    }

    if (add_colors(fp))
      fprintf(fp, RESET_COLOR);

    fflush(fp);
    if (level > most_severe_log)
      most_severe_log = level;
  }
}

void debug(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  colored_log(stdout, LL_DEBUG, fmt, ap);
  va_end(ap);
}

void info(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  colored_log(stdout, LL_INFO, fmt, ap);
  va_end(ap);
}

void warn(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  colored_log(stderr, LL_WARN, fmt, ap);
  va_end(ap);
}

typedef void (*logger_sig_fn)(const char *fmt, ...);

void error(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  colored_log(stderr, LL_ERROR, fmt, ap);
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
    // If exactly one element would be truncated, just print it instead of
    // truncating
    bottom -= 1;
  } else if (top != bottom) {
    // avoid printing the separator if top and bottom exactly meet
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
  colored_log(stderr, LL_FATAL, fmt, ap);
  va_end(ap);

  if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
    colored_log(stderr, LL_FATAL, strerror(errno), NULL);
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

static void print_ctx(logger_sig_fn f, parse_context *ctx) {
  int start, end;
  start = ctx->c;
  while (start > 0 && ctx->src[start] != '\n')
    start--;
  if (ctx->src[start] == '\n')
    start++;

  end = ctx->c;
  while (end < ctx->n && ctx->src[end] != '\n')
    end++;
  f("%.*s\n"
    "%*s",
    end - start, ctx->src + start, ctx->c - start + 1, "^");
}

void error_ctx(parse_context *ctx) { print_ctx(error, ctx); }

void warn_ctx(parse_context *ctx) { print_ctx(warn, ctx); }

void debug_ctx(parse_context *ctx) { print_ctx(debug, ctx); }

void info_ctx(parse_context *ctx) { print_ctx(info, ctx); }

static bool init = false;
void setup_crash_stacktrace_logger(void) {
  if (!init) {
    atexit(destroy_strbuf);
    init = true;
    int signals[] = {SIGINT, SIGSEGV, SIGTERM, SIGHUP, SIGQUIT};
    for (int i = 0; i < LENGTH(signals); i++)
      sigaction(signals[i], &(struct sigaction){.sa_handler = handler}, NULL);
  }
}
