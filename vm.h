#ifndef EVE_VM_H
#define EVE_VM_H

#include <math.h>

#include "code.h"
#include "debug.h"
#include "table.h"
#include "util.h"
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
  Value* sp;
  Obj* objects;
  Table strings;
} VM;

VM new_vm();
void init_vm(VM* vm, Code* code);
IResult run(VM* vm);
Value create_string(VM* vm, Table* table, char* str, int len);

#endif  //EVE_VM_H
