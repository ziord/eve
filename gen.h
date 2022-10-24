#ifndef EVE_GEN_H
#define EVE_GEN_H
#include "compiler.h"
void emit_byte(Compiler* co, byte_t byte, int line);
void emit_value(Compiler* co, byte_t opcode, Value val, int line);
#endif  //EVE_GEN_H
