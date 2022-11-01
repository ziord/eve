#include "code.h"

void init_code(Code *code) {
  code->lines = NULL;
  code->bytes = NULL;
  code->length = 0;
  code->capacity = 0;
  init_value_pool(&code->vpool);
}

void free_code(Code *code, VM *vm) {
  FREE(vm, code->bytes, byte_t);
  FREE(vm, code->lines, int);
  free_value_pool(&code->vpool, vm);
  init_code(code);
}

void write_code(Code *code, byte_t byte, int line, VM *vm) {
  if (code->length >= code->capacity) {
    code->capacity = GROW_CAPACITY(code->capacity);
    code->bytes = GROW_BUFFER(vm, code->bytes, byte_t, code->length,
                              code->capacity);
    code->lines =
        GROW_BUFFER(vm, code->lines, int, code->length, code->capacity);
  }
  code->bytes[code->length] = byte;
  code->lines[code->length++] = line;
}
