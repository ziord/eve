#ifndef EVE_VM_H
#define EVE_VM_H

#include "code.h"
#include "util.h"
#include "debug.h"
#define STACK_MAX 0xfff

typedef enum {
  RESULT_SUCCESS,  // successful run
  RESULT_RUNTIME_ERROR,  // runtime error
  RESULT_COMPILE_ERROR,  // runtime error
} IResult;

typedef struct VM {
  byte_t* ip;
  Code* code;
  Value stack[STACK_MAX];
  Value *sp;
} VM;

void init_vm(VM* vm, Code* code);
IResult run(VM* vm);

#endif  //EVE_VM_H
