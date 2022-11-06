#ifndef EVE_COMPILER_H
#define EVE_COMPILER_H

#include "code.h"
#include "parser.h"

typedef struct {
  bool initialized;
  int scope;
  int name_len;
  char* name;
} LVar;

typedef struct {
  int errors;
  int scope;
  int locals_count;
  AstNode* root;
  Code* code;
  LVar locals[CONST_MAX];
  VM* vm;
} Compiler;

Compiler new_compiler(AstNode* node, Code* code, VM* vm);
void compile(Compiler* compiler);

#endif  //EVE_COMPILER_H
