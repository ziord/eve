#ifndef EVE_VM_H
#define EVE_VM_H

#include <math.h>

#include "defs.h"
#include "gc.h"
#include "util.h"

#ifdef EVE_DEBUG
  #include "debug.h"
#endif

#define CALL_FRAME_MAX (0x50)
#define TRY_FRAME_MAX (UINT16_MAX)
#define STACK_MAX ((CALL_FRAME_MAX) * (CONST_MAX + 1))

typedef enum {
  RESULT_SUCCESS = 0,  // successful run
  RESULT_COMPILE_ERROR,  // parse/compile error
  RESULT_RUNTIME_ERROR,  // runtime error
} IResult;

typedef struct {
  byte_t* handler_ip;
  Value* sp;
} TryCtx;

typedef struct {
  byte_t* ip;
  TryCtx* try_ctx;
  ObjClosure* closure;
  Value* stack;
} CallFrame;

typedef struct VM {
  bool is_compiling;
  bool has_error;
  int frame_count;
  int try_count;
  ObjHashMap strings;
  ObjHashMap modules;
  GC gc;
  Value stack[STACK_MAX];
  CallFrame frames[CALL_FRAME_MAX];
  TryCtx try_ctxs[TRY_FRAME_MAX];
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
bool vm_call_value(VM* vm, Value val, int argc);
VM new_vm();
void free_vm(VM* vm);
void boot_vm(VM* vm, ObjFn* func);
IResult runtime_error(VM* vm, Value err, char* fmt, ...);
IResult run(VM* vm);
void serde_error_cb(VM* vm, char* fmt, ...);

#endif  //EVE_VM_H
