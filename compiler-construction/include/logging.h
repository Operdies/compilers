#include "text.h"
enum loglevel {
  FATAL,
  ERROR,
  WARN,
  INFO,
  DEBUG,
};

void debug(const char *fmt, ...);
void info(const char *fmt, ...);
void warn(const char *fmt, ...);
void error(const char *fmt, ...);
void die(const char *fmt, ...) __attribute__((__noreturn__));
int set_loglevel(enum loglevel level);
int get_loglevel(void);
void setup_crash_stacktrace_logger(void);

void error_ctx(parse_context *ctx);
void warn_ctx(parse_context *ctx);
void debug_ctx(parse_context *ctx);
void info_ctx(parse_context *ctx);
