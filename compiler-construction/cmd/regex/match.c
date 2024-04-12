// link: regex.o arena.o collections.o logging.o
#include "regex.h"
#include <stdio.h>

int main(int argc, char *argv[argc+1]){
  for (int i = 1; (i+1) < argc; i+=2){
    char *pattern = argv[i];
    char *test = argv[i+1];
    printf("Testing pattern %s with test %s\n", pattern, test);
    bool is_match = matches(pattern, test);
    printf("Match: %s\n", is_match ? "true" : "false");
  }
  return 0;
}
