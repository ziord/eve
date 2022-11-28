#ifndef EVE_OPCODE_H
#define EVE_OPCODE_H

#include "common.h"
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
  $BUILD_MAP,
  $BUILD_CLOSURE,
  $BUILD_STRUCT,
  $BUILD_INSTANCE,
  $POP,
  $POP_N,
  $DISPLAY,
  $SUBSCRIPT,
  $GET_FIELD,  // struct/module
  $GET_PROPERTY,  // instance
  $SET_PROPERTY,  // instance
  $SET_SUBSCRIPT,
  $DEFINE_GLOBAL,
  $GET_GLOBAL,
  $GET_LOCAL,
  $SET_GLOBAL,
  $SET_LOCAL,
  $GET_UPVALUE,
  $SET_UPVALUE,
  $CLOSE_UPVALUE,
  $ASSERT,
  $PUSH_TRY,
  $POP_TRY,
  $THROW,
  $LOOP,
  $CALL,
  $TAIL_CALL,
  $RET
} OpCode;

#endif  //EVE_OPCODE_H
