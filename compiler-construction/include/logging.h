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
void die(const char *fmt, ...);
int set_loglevel(enum loglevel level);
int get_loglevel(void);
void setup_crash_stacktrace_logger(void);
