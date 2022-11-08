#include "gen.h"

extern void compile_error(Compiler* compiler, char* fmt, ...);

void emit_byte(Compiler* co, byte_t opcode, int line) {
  write_code(co->code, opcode, line, co->vm);
}

void emit_value(Compiler* co, byte_t opcode, Value val, int line) {
  int index = write_value(&co->code->vpool, val, co->vm);
  ASSERT_MAX(
      co,
      index,
      CONST_MAX,
      "Too many constants. Maximum constant size exceeded");
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

void emit_loop(Compiler* co, int offset, int line) {
  int jmp_offset = co->code->length - offset
      + 3;  // +3 to include the opcode and its 2 bytes offset operand
  ASSERT_MAX(
      co,
      jmp_offset,
      UINT16_MAX,
      "Code body too large to loop over: %d",
      jmp_offset);
  emit_byte(co, $LOOP, line);
  emit_byte(co, (jmp_offset >> 8) & 0xff, line);
  emit_byte(co, jmp_offset & 0xff, line);
}

void patch_jump(Compiler* co, int index) {
  // calc how much we need to jump
  int jmp_offset =
      co->code->length - index - 2;  // -2 to exclude the 2-byte operand
  ASSERT_MAX(
      co,
      jmp_offset,
      UINT16_MAX,
      "Code body too large to jump over: %d",
      jmp_offset);
  co->code->bytes[index++] = (jmp_offset >> 8) & 0xff;
  co->code->bytes[index] = jmp_offset & 0xff;
}

inline int last_line(Compiler* co) {
  if (co->code->length) {
    return co->code->lines[co->code->length - 1];
  }
  return -1;
}