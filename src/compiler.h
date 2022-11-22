#ifndef EVE_COMPILER_H
#define EVE_COMPILER_H

#include "parser.h"
#include "value.h"

#define MAX_CONTROLS UINT16_MAX

typedef struct {
  bool initialized;
  bool is_captured;
  int scope;
  int name_len;
  char* name;
} LocalVar;

typedef struct {
  int scope;
  ControlStmtNode* node;
} LoopVar;

typedef struct {
  bool is_local;
  int index;
} Upvalue;

typedef struct Compiler {
  int errors;
  int scope;
  int locals_count;
  int controls_count;
  int upvalues_count;
  int free_vars;
  LocalVar locals[CONST_MAX];
  Upvalue upvalues[CONST_MAX];
  LoopVar controls[MAX_CONTROLS];
  LoopVar current_loop;
  VM* vm;
  AstNode* root;
  ObjFn* func;
  struct Compiler* enclosing;
  ObjStruct* module;
} Compiler;

void new_compiler(
    Compiler* compiler,
    AstNode* node,
    ObjFn* func,
    VM* vm,
    char* fpath);
void compile(Compiler* compiler);

#endif  //EVE_COMPILER_H
