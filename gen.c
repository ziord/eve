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

int emit_jump(Compiler* co, byte_t opcode, int line) {
  // save a jmp slot for future rewrite
  emit_byte(co, opcode, line);
  int index = co->code->length;
  emit_byte(co, 0xff, line);
  emit_byte(co, 0xff, line);
  return index;
}

void patch_jump(Compiler* co, int index) {
  // calc how much we need to jump
  int jmp_offset =
      co->code->length - index - 2;  // -2 to exclude the 2-byte operand
  co->code->bytes[index++] = (jmp_offset >> 8) & 0xff;
  co->code->bytes[index] = jmp_offset & 0xff;
}