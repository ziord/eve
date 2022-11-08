#ifndef EVE_GEN_H
#define EVE_GEN_H
#include "compiler.h"

#define ASSERT_MAX(co, val, max, ...) \
  if ((val) > (max)) { \
    compile_error((co), __VA_ARGS__); \
    return; \
  }

void emit_byte(Compiler* co, byte_t byte, int line);
void emit_value(Compiler* co, byte_t opcode, Value val, int line);
int emit_jump(Compiler* co, byte_t opcode, int line);
void patch_jump(Compiler* co, int index);
void emit_loop(Compiler* co, int offset, int line);
int last_line(Compiler* co);
#endif  //EVE_GEN_H
