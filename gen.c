#include "gen.h"

void emit_byte(Compiler* co, byte_t opcode, int line) {
  write_code(co->code, opcode, line, co->vm);
}

void emit_value(Compiler* co, byte_t opcode, Value val, int line) {
  int index = write_value(&co->code->vpool, val, co->vm);
  if (index > BYTE_MAX) {
    FAIL("Too many constants", "Maximum constant size exceeded");
  }
  emit_byte(co, opcode, line);
  emit_byte(co, CAST(byte_t, index), line);
}
