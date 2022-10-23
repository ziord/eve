#include "memory.h"

void *alloc(VM *vm, void *ptr, size_t curr_size, size_t new_size,
            char *fmt, ...) {
  (void) curr_size;
  if (new_size == 0) {
    free(ptr);
    return NULL;
  }
  void *tmp = realloc(ptr, new_size);
  if (!tmp) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
  }
  return tmp;
}