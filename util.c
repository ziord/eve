#include "util.h"

#include <errno.h>

_Noreturn void error(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

int copy_str(const char* src, char** dest, int len) {
  int i, real_len;
  for (i = 0, real_len = 0; i < len; i++) {
    if (src[i] != '\\') {
      (*dest)[real_len] = src[i];
    } else {
      switch (src[i + 1]) {
        case '\\':
          (*dest)[real_len] = '\\';
          break;
        case '"':
          (*dest)[real_len] = '"';
          break;
        case '\'':
          (*dest)[real_len] = '\'';
          break;
        case 'r':
          (*dest)[real_len] = '\r';
          break;
        case 't':
          (*dest)[real_len] = '\t';
          break;
        case 'n':
          (*dest)[real_len] = '\n';
          break;
        case 'f':
          (*dest)[real_len] = '\f';
          break;
        case 'b':
          (*dest)[real_len] = '\b';
          break;
        case 'a':
          (*dest)[real_len] = '\a';
          break;
        default:
          // TODO: better error handling
          error("unknown escape sequence");
      }
      i++;
    }
    real_len++;
  }
  return real_len;
}

int copy_str_compact(VM* vm, const char* src, char** dest, int len) {
  int real_len = copy_str(src, dest, len);
  if ((len - real_len) >= BUFFER_INIT_SIZE) {
    // trim the string
    *dest = GROW_BUFFER(vm, *dest, char, len, real_len);
  }
  return real_len;
}

char* read_file(const char* fn) {
  if (!fn) {
    error("No filename specified");
  }
  FILE* file = fopen(fn, "r");
  if (!file) {
    error("Could not open file '%s' <errno: %d>", fn, errno);
  }
  fseek(file, 0L, SEEK_END);
  size_t size = ftell(file);
  rewind(file);
  char* tmp = alloc(NULL, size + 1);
  if (fread(tmp, sizeof(char), size, file) < size) {
    if (ferror(file)) {
      clearerr(file);
    }
    fclose(file);
    error("Could not read file '%s' <errno: %d>", fn, errno);
  }
  tmp[size] = '\0';
  fclose(file);
  return tmp;
}
