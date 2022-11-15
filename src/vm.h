#ifndef EVE_VM_H
#define EVE_VM_H

#include <math.h>

#include "debug.h"
#include "util.h"

#define FRAME_MAX (0x50)
#define STACK_MAX ((FRAME_MAX) * (CONST_MAX + 1))

typedef enum {
  RESULT_SUCCESS = 0,  // successful run
  RESULT_COMPILE_ERROR,  // parse/compile error
  RESULT_RUNTIME_ERROR,  // runtime error
} IResult;

typedef struct {
  byte_t* ip;
  ObjClosure* closure;
  Value* stack;
} CallFrame;

typedef struct VM {
  int frame_count;
  size_t bytes_alloc;
  ObjHashMap strings;
  ObjHashMap globals;
  Value stack[STACK_MAX];
  CallFrame frames[FRAME_MAX];
  CallFrame* fp;
  Value* sp;
  Obj* objects;
} VM;

VM new_vm();
void free_vm(VM* vm);
void boot_vm(VM* vm, ObjFn* func);
IResult run(VM* vm);

#endif  //EVE_VM_H
