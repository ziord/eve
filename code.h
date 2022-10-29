#ifndef EVE_CODE_H
#define EVE_CODE_H
#include "memory.h"
#include "value.h"

typedef uint8_t byte_t;
typedef enum {
  // Arith
  $ADD,
  $SUBTRACT,
  $MULTIPLY,
  $DIVIDE,
  $NEGATE,
  $MOD,
  $POW,
  // Conditional
  $NOT,
  $EQ,
  $NOT_EQ,
  $LESS,
  $GREATER,
  $LESS_OR_EQ,
  $GREATER_OR_EQ,
  $JMP,
  $JMP_FALSE,
  $JMP_FALSE_OR_POP,
  // Bitwise
  $BW_LSHIFT,
  $BW_RSHIFT,
  $BW_INVERT,
  $BW_AND,
  $BW_XOR,
  $BW_OR,
  // Other
  $LOAD_CONST,
  $BUILD_LIST,
  $RET
} OpCode;
typedef struct {
  int length;
  int capacity;
  int* lines;
  byte_t* bytes;
  ValuePool vpool;
} Code;

void init_code(Code* code);
void free_code(Code* code, VM* vm);
void write_code(Code* code, byte_t byte, int line, VM* vm);

#endif  //EVE_CODE_H
