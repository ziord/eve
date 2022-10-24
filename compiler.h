#ifndef EVE_COMPILER_H
#define EVE_COMPILER_H

#include "code.h"
#include "parser.h"

typedef struct {
  AstNode* root;
  Code* code;
  VM* vm;
} Compiler;

Compiler new_compiler(AstNode* node, Code* code, VM* vm);
void compile(Compiler* compiler);

#endif  //EVE_COMPILER_H
