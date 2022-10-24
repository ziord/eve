#include "debug.h"

int plain_instruction(char* inst, int offset) {
  printf("%s\n", inst);
  return ++offset;
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
    case $RET:
      return plain_instruction("$RET", index);
    case $LOAD_CONST:
      return constant_instruction("$LOAD_CONST", code, index);
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