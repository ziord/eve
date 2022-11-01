#ifndef EVE_GEN_H
#define EVE_GEN_H
#include "compiler.h"
void emit_byte(Compiler* co, byte_t byte, int line);
void emit_value(Compiler* co, byte_t opcode, Value val, int line);
int emit_jump(Compiler* co, byte_t opcode, int line);
void patch_jump(Compiler* co, int index);
#endif  //EVE_GEN_H
