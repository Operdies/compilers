#ifndef __LOGGING_H
#define __LOGGING_H

#include "text.h"
enum loglevel {
  DEBUG,
  INFO,
  WARN,
  ERROR,
  FATAL,
};

void debug(const char *fmt, ...);
void info(const char *fmt, ...);
void warn(const char *fmt, ...);
void error(const char *fmt, ...);
void die(const char *fmt, ...) __attribute__((__noreturn__));
enum loglevel set_loglevel(enum loglevel level);
enum loglevel get_loglevel(void);
enum loglevel log_severity(void);
void setup_crash_stacktrace_logger(void);

void error_ctx(parse_context *ctx);
void warn_ctx(parse_context *ctx);
void debug_ctx(parse_context *ctx);
void info_ctx(parse_context *ctx);

#endif
