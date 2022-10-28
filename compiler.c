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
    [OP_LESS] = {0xff, $LESS},
    [OP_LESS_EQ] = {0xff, $LESS_OR_EQ},
    [OP_GRT] = {0xff, $GREATER},
    [OP_GRT_EQ] = {0xff, $GREATER_OR_EQ},
    [OP_LOG_EQ] = {0xff, $EQ},
    [OP_NOT_EQ] = {0xff, $NOT_EQ},
    [OP_LSHIFT] = {0xff, $BW_LSHIFT},
    [OP_RSHIFT] = {0xff, $BW_RSHIFT},
    [OP_BW_AND] = {0xff, $BW_AND},
    [OP_BW_OR] = {0xff, $BW_OR},
    [OP_BW_XOR] = {0xff, $BW_XOR},
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

void c_unit(Compiler* compiler, AstNode* node) {
  UnitNode* unit = CAST(UnitNode*, node);
  if (unit->is_bool) {
    emit_value(
        compiler,
        $LOAD_CONST,
        unit->is_true_bool ? TRUE_VAL : FALSE_VAL,
        unit->line);
  } else if (unit->is_none) {
    emit_value(compiler, $LOAD_CONST, NONE_VAL, unit->line);
  }
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

void c_and(Compiler* compiler, BinaryNode* node) {
  // x && y => x == false ? skip y
  c_(compiler, node->l_node);
  int idx = emit_jump(compiler, $JMP_FALSE_OR_POP, node->line);
  c_(compiler, node->r_node);
  patch_jump(compiler, idx);
}

void c_or(Compiler* compiler, BinaryNode* node) {
  // x || y => x == true ? skip y
  c_(compiler, node->l_node);
  int skip_jmp_idx = emit_jump(compiler, $JMP_FALSE, node->line);
  int end_jmp_idx = emit_jump(compiler, $JMP, node->line);
  patch_jump(compiler, skip_jmp_idx);
  c_(compiler, node->r_node);
  patch_jump(compiler, end_jmp_idx);
}

void c_binary(Compiler* compiler, AstNode* node) {
  BinaryNode* bin = CAST(BinaryNode*, node);
  if (bin->op == OP_OR) {
    c_or(compiler, bin);
  } else if (bin->op == OP_AND) {
    c_and(compiler, bin);
  } else {
    c_(compiler, bin->l_node);
    c_(compiler, bin->r_node);
    ASSERT(op_table[bin->op][1] != 0xff, "binary opcode should be defined");
    emit_byte(compiler, op_table[bin->op][1], bin->line);
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
    case AST_UNARY:
      c_unary(compiler, node);
      break;
    case AST_UNIT:
      c_unit(compiler, node);
      break;
    default:
      UNREACHABLE("compile - unknown ast node");
  }
}

void compile(Compiler* compiler) {
  c_(compiler, compiler->root);
  emit_byte(compiler, $RET, 100);
}