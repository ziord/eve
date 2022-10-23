#ifndef EVE_CODE_H
#define EVE_CODE_H
#include "memory.h"
#include "value.h"

typedef uint8_t byte_t;
typedef enum { $ADD, $SUBTRACT, $MULTIPLY, $DIVIDE, $LOAD_CONST, $RET } OpCode;
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
