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
    case $JMP:
      return jump_instruction("$JMP", code, index, 1);
    case $JMP_FALSE:
      return jump_instruction("$JMP_FALSE", code, index, 1);
    case $JMP_FALSE_OR_POP:
      return jump_instruction("$JMP_FALSE_OR_POP", code, index, 1);
    case $LOAD_CONST:
      return constant_instruction("$LOAD_CONST", code, index);
    case $BUILD_LIST:
      return byte_instruction("$BUILD_LIST", code, index);
    case $BUILD_MAP:
      return byte_instruction("$BUILD_MAP", code, index);
    case $DISPLAY:
      return byte_instruction("$DISPLAY", code, index);
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