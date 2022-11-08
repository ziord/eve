#ifndef EVE_COMPILER_H
#define EVE_COMPILER_H

#include "code.h"
#include "parser.h"

#define MAX_CONTROLS UINT16_MAX

typedef struct {
  bool initialized;
  int scope;
  int name_len;
  char* name;
} LocalVar;

typedef struct {
  int scope;
  ControlStmtNode* node;
} LoopVar;

typedef struct {
  int errors;
  int scope;
  int locals_count;
  int controls_count;
  AstNode* root;
  Code* code;
  LocalVar locals[CONST_MAX];
  LoopVar controls[MAX_CONTROLS];
  LoopVar current_loop;
  VM* vm;
} Compiler;

Compiler new_compiler(AstNode* node, Code* code, VM* vm);
void compile(Compiler* compiler);

#endif  //EVE_COMPILER_H
