#include "compiler.h"

#include "gen.h"

OpCode op_table[][2] = {
    // op         unary       binary
    [OP_PLUS] = {0xff, $ADD},
    [OP_MUL] = {0xff, $MULTIPLY},
    [OP_DIV] = {0xff, $DIVIDE},
    [OP_MINUS] = {$NEGATE, $SUBTRACT},
    [OP_MOD] = {0xff, $MOD},
    [OP_NOT] = {$NOT, 0xff},
    [OP_POW] = {0xff, $POW},
    [OP_BW_COMPL] = {$BW_INVERT, 0xff},
};

void c_(Compiler* compiler, AstNode* node);

Compiler new_compiler(AstNode* node, Code* code, VM* vm) {
  Compiler compiler = {.root = node, .code = code, .vm = vm};
  return compiler;
}

void c_num(Compiler* compiler, AstNode* node) {
  NumberNode* num = CAST(NumberNode*, node);
  emit_value(compiler, $LOAD_CONST, NUMBER_VAL(num->value), num->line);
}

void c_unary(Compiler* compiler, AstNode* node) {
  UnaryNode* unary = CAST(UnaryNode*, node);
  c_(compiler, unary->node);
  if (unary->op != OP_PLUS) {
    ASSERT(
        op_table[unary->op][0] != 0xff,
        "unary opcode should be defined");
    emit_byte(compiler, op_table[unary->op][0], unary->line);
  }
}

void c_binary(Compiler* compiler, AstNode* node) {
  BinaryNode* bin = CAST(BinaryNode*, node);
  c_(compiler, bin->l_node);
  c_(compiler, bin->r_node);
  ASSERT(op_table[bin->op][1] != 0xff, "binary opcode should be defined");
  emit_byte(compiler, op_table[bin->op][1], bin->line);
}

void c_(Compiler* compiler, AstNode* node) {
  AstTy ty = *((AstTy*)node);
  switch (ty) {
    case AST_NUM:
      c_num(compiler, node);
      break;
    case AST_BINARY:
      c_binary(compiler, node);
      break;
    case AST_UNARY:
      c_unary(compiler, node);
      break;
    default:
      UNREACHABLE("compile - unknown ast node");
  }
}

void compile(Compiler* compiler) {
  c_(compiler, compiler->root);
  emit_byte(compiler, $RET, 100);
}