#include "debug.h"

int plain_instruction(char* inst, int offset) {
  printf("%s\n", inst);
  return ++offset;
}

int byte_instruction(char* inst, Code* code, int offset) {
  printf("%-16s\t%3d\n", inst, code->bytes[offset + 1]);
  return offset + 2;
}

int constant_instruction(char* inst, Code* code, int offset) {
  // inst, operand, (value).
  int operand = code->bytes[++offset];
  printf("%-16s\t%3d    ", inst, operand);
  printf("(");
  Value val = code->vpool.values[operand];
  print_value(val);
  printf(")\n");
  return ++offset;
}

int jump_instruction(char* inst, Code* code, int offset, int sign) {
  // jmp offset -> op-arg (2 bytes)
  int jmp_offset = code->bytes[offset + 1] << 8;
  jmp_offset |= code->bytes[offset + 2];
  printf(
      "%-16s\t%3d -> %d\n",
      inst,
      offset,
      (jmp_offset * sign) + offset + 3);
  return offset + 3;
}

int closure_instruction(char* inst, Code* code, int offset) {
  int slot = code->bytes[offset + 1];
  ObjFn* fn = AS_FUNC(code->vpool.values[slot]);
  offset = constant_instruction(inst, code, offset);
  for (int i = 0; i < fn->env_len; i++) {
    // index, is_local
    int uv_index = code->bytes[offset++];
    bool uv_is_local = code->bytes[offset++];
    printf(
        "   |\t%04d\t%-16s\t\t   upvalue  %d  %d\n",
        offset,
        " | ",
        uv_index,
        uv_is_local);
  }
  return offset;
}

int struct_instruction(char* inst, Code* code, int offset) {
  // inst name-slot-index field-count
  int operand = code->bytes[++offset];
  printf("%-16s\t%3d    ", inst, operand);
  printf("(");
  Value val = code->vpool.values[operand];
  print_value(val);
  printf(")\t %d\n", code->bytes[++offset]);
  return ++offset;
}

int dis_instruction(Code* code, int index) {
  if (index > 0 && code->lines[index] == code->lines[index - 1]) {
    printf("   |\t%04d\t", index);
  } else {
    printf("%4d\t%04d\t", code->lines[index], index);
  }
  byte_t byte = code->bytes[index];
  switch (byte) {
    case $ADD:
      return plain_instruction("$ADD", index);
    case $MULTIPLY:
      return plain_instruction("$MULTIPLY", index);
    case $DIVIDE:
      return plain_instruction("$DIVIDE", index);
    case $SUBTRACT:
      return plain_instruction("$SUBTRACT", index);
    case $NEGATE:
      return plain_instruction("$NEGATE", index);
    case $MOD:
      return plain_instruction("$MOD", index);
    case $POW:
      return plain_instruction("$POW", index);
    case $NOT:
      return plain_instruction("$NOT", index);
    case $BW_INVERT:
      return plain_instruction("$BW_INVERT", index);
    case $LESS:
      return plain_instruction("$LESS", index);
    case $LESS_OR_EQ:
      return plain_instruction("$LESS_OR_EQ", index);
    case $GREATER:
      return plain_instruction("$GREATER", index);
    case $GREATER_OR_EQ:
      return plain_instruction("$GREATER_OR_EQ", index);
    case $EQ:
      return plain_instruction("$EQ", index);
    case $NOT_EQ:
      return plain_instruction("$NOT_EQ", index);
    case $BW_LSHIFT:
      return plain_instruction("$BW_LSHIFT", index);
    case $BW_RSHIFT:
      return plain_instruction("$BW_RSHIFT", index);
    case $BW_AND:
      return plain_instruction("$BW_AND", index);
    case $BW_OR:
      return plain_instruction("$BW_OR", index);
    case $BW_XOR:
      return plain_instruction("$BW_XOR", index);
    case $RET:
      return plain_instruction("$RET", index);
    case $POP:
      return plain_instruction("$POP", index);
    case $CLOSE_UPVALUE:
      return plain_instruction("$CLOSE_UPVALUE", index);
    case $SUBSCRIPT:
      return plain_instruction("$SUBSCRIPT", index);
    case $SET_SUBSCRIPT:
      return plain_instruction("$SET_SUBSCRIPT", index);
    case $TEAR_TRY:
      return plain_instruction("$TEAR_TRY", index);
    case $THROW:
      return plain_instruction("$THROW", index);
    case $ASSERT:
      return plain_instruction("$ASSERT", index);
    case $JMP:
      return jump_instruction("$JMP", code, index, 1);
    case $JMP_FALSE:
      return jump_instruction("$JMP_FALSE", code, index, 1);
    case $JMP_FALSE_OR_POP:
      return jump_instruction("$JMP_FALSE_OR_POP", code, index, 1);
    case $LOOP:
      return jump_instruction("$LOOP", code, index, -1);
    case $SET_TRY:
      return jump_instruction("$SET_TRY", code, index, 1);
    case $BUILD_LIST:
      return byte_instruction("$BUILD_LIST", code, index);
    case $BUILD_MAP:
      return byte_instruction("$BUILD_MAP", code, index);
    case $CALL:
      return byte_instruction("$CALL", code, index);
    case $TAIL_CALL:
      return byte_instruction("$TAIL_CALL", code, index);
    case $DISPLAY:
      return byte_instruction("$DISPLAY", code, index);
    case $POP_N:
      return byte_instruction("$POP_N", code, index);
    case $SET_LOCAL:
      return byte_instruction("$SET_LOCAL", code, index);
    case $SET_UPVALUE:
      return byte_instruction("$SET_UPVALUE", code, index);
    case $GET_LOCAL:
      return byte_instruction("$GET_LOCAL", code, index);
    case $GET_UPVALUE:
      return byte_instruction("$GET_UPVALUE", code, index);
    case $BUILD_INSTANCE:
      return byte_instruction("$BUILD_INSTANCE", code, index);
    case $SET_PROPERTY:
      return byte_instruction("$SET_PROPERTY", code, index);
    case $LOAD_CONST:
      return constant_instruction("$LOAD_CONST", code, index);
    case $DEFINE_GLOBAL:
      return constant_instruction("$DEFINE_GLOBAL", code, index);
    case $GET_GLOBAL:
      return constant_instruction("$GET_GLOBAL", code, index);
    case $SET_GLOBAL:
      return constant_instruction("$SET_GLOBAL", code, index);
    case $GET_PROPERTY:
      return constant_instruction("$GET_PROPERTY", code, index);
    case $GET_FIELD:
      return constant_instruction("$GET_FIELD", code, index);
    case $BUILD_STRUCT:
      return struct_instruction("$BUILD_STRUCT", code, index);
    case $BUILD_CLOSURE:
      return closure_instruction("$BUILD_CLOSURE", code, index);
    default:
      return plain_instruction("UNKNOWN_OPCODE", index);
  }
}

void dis_code(Code* code, char* name) {
  printf(">>Disassembly of %s<<\n", name);
  for (int i = 0; i < code->length;) {
    i = dis_instruction(code, i);
  }
}