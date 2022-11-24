#ifndef EVE_VM_H
#define EVE_VM_H

#include <math.h>

#include "debug.h"
#include "gc.h"
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
  bool is_compiling;
  bool has_error;
  int frame_count;
  ObjHashMap strings;
  ObjHashMap modules;
  GC gc;
  Value stack[STACK_MAX];
  CallFrame frames[FRAME_MAX];
  CallFrame* fp;
  Value* sp;
  ObjUpvalue* upvalues;
  Obj* objects;
  struct Compiler* compiler;
  ObjStruct* builtins;
  ObjStruct* current_module;
} VM;

Value vm_pop_stack(VM* vm);
void vm_push_stack(VM* vm, Value val);
bool vm_push_frame(VM* vm, CallFrame frame);
VM new_vm();
void free_vm(VM* vm);
void boot_vm(VM* vm, ObjFn* func);
IResult runtime_error(VM* vm, char* fmt, ...);
IResult run(VM* vm);

#endif  //EVE_VM_H
