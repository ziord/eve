#ifndef EVE_VM_H
#define EVE_VM_H

#include <math.h>

#include "code.h"
#include "debug.h"
#include "util.h"
#define STACK_MAX 0xfff

typedef enum {
  RESULT_SUCCESS = 0,  // successful run
  RESULT_COMPILE_ERROR,  // parse/compile error
  RESULT_RUNTIME_ERROR,  // runtime error
} IResult;

typedef struct VM {
  byte_t* ip;
  Code* code;
  Value stack[STACK_MAX];
  Value* sp;
  Obj* objects;
  size_t bytes_alloc;
  ObjHashMap strings;
  ObjHashMap globals;
} VM;

VM new_vm();
void boot_vm(VM* vm, Code* code);
IResult run(VM* vm);

#endif  //EVE_VM_H
