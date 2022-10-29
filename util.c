#include "util.h"

_Noreturn void error(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

int copy_str(VM* vm, const char* src, char** dest, int len) {
  int i, real_len;
  for (i = 0, real_len = 0; i < len; i++) {
    if (src[i] != '\\') {
      (*dest)[i] = src[i];
    } else {
      switch (src[i + 1]) {
        case '\\':
          (*dest)[i] = '\\';
          break;
        case '"':
          (*dest)[i] = '"';
          break;
        case '\'':
          (*dest)[i] = '\'';
          break;
        case 'r':
          (*dest)[i] = '\r';
          break;
        case 't':
          (*dest)[i] = '\t';
          break;
        case 'n':
          (*dest)[i] = '\n';
          break;
        case 'f':
          (*dest)[i] = '\f';
          break;
        case 'b':
          (*dest)[i] = '\b';
          break;
        case 'a':
          (*dest)[i] = '\a';
          break;
        default:
          // TODO: better error handling
          error("unknown escape sequence");
      }
      i++;
    }
    real_len++;
  }
  // TODO: necessary?
  if ((len - real_len) > 5) {
    // trim the string
    void* tmp = realloc(*dest, i);
    if (tmp) {
      FREE(vm, *dest, char);
      *dest = tmp;
    }
  }
  return real_len;
}