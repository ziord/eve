#include "compiler.h"

#include "gen.h"

OpCode op_table[] = {
    [OP_PLUS] = $ADD,
    [OP_MUL] = $MULTIPLY,
    [OP_DIV] = $DIVIDE,
    [OP_MINUS] = $SUBTRACT,
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

void c_binary(Compiler* compiler, AstNode* node) {
  BinaryNode* binary = CAST(BinaryNode*, node);
  c_(compiler, binary->l_node);
  c_(compiler, binary->r_node);
  switch (binary->op) {
    case OP_PLUS:
    case OP_MUL:
    case OP_DIV:
    case OP_MINUS:
      emit_byte(compiler, op_table[binary->op], binary->line);
      break;
    default:
      UNREACHABLE("c_binary - unknown op type");
  }
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
    case AST_STR:
      break;
    case AST_UNARY:
      break;
    default:
      UNREACHABLE("compile - unknown ast node");
  }
}

void compile(Compiler* compiler) {
  c_(compiler, compiler->root);
  emit_byte(compiler, $RET, 100);
}