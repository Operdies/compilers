#ifndef TEXT_H
#define TEXT_H

#define finished(ctx) ((ctx)->c >= (ctx)->n)
#define peek(ctx) (finished((ctx)) ? -1 : (ctx)->src[(ctx)->c])
#define take(ctx) (finished((ctx)) ? -1 : (ctx)->src[(ctx)->c++])
#define advance(ctx) ((ctx)->c++);

typedef struct {
  int c;            // cursor
  int n;            // length
  const char *src;  // text being parsed
} parse_context;

#define SINGLETICK_STR "'([^'\\\\]|\\\\.)*'"
#define DOUBLETICK_STR "\"([^\"\\\\]|\\\\.)*\""
static const char string_regex[] = SINGLETICK_STR "|" DOUBLETICK_STR;

#endif  // TEXT_H
