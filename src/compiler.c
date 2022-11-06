#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
#include "compiler.h"

#include "gen.h"
#include "vm.h"

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

static void compile_error(Compiler* compiler, AstNode* node, char* msg) {
  (void)node;
  error(msg);
}

static int store_variable(Compiler* compiler, VarNode* var) {
  Value val = create_string(
      compiler->vm,
      &compiler->vm->strings,
      var->name,
      var->len,
      false);
  int slot = write_value(&compiler->code->vpool, val, compiler->vm);
  return slot;
}

void c_num(Compiler* compiler, AstNode* node) {
  NumberNode* num = CAST(NumberNode*, node);
  emit_value(compiler, $LOAD_CONST, NUMBER_VAL(num->value), num->line);
}

void c_str(Compiler* compiler, AstNode* node) {
  StringNode* str = CAST(StringNode*, node);
  Value str_val = create_string(
      compiler->vm,
      &compiler->vm->strings,
      str->start,
      str->length,
      str->is_alloc);
  emit_value(compiler, $LOAD_CONST, str_val, str->line);
}

void c_list(Compiler* compiler, AstNode* node) {
  ListNode* list = CAST(ListNode*, node);
  // push arguments in reverse order
  for (int i = list->len - 1; i >= 0; i--) {
    c_(compiler, list->elems[i]);
  }
  emit_byte(compiler, $BUILD_LIST, list->line);
  emit_byte(compiler, (byte_t)list->len, list->line);
}

void c_map(Compiler* compiler, AstNode* node) {
  MapNode* map = CAST(MapNode*, node);
  // push arguments in reverse order
  for (int i = map->length - 1; i >= 0; i--) {
    c_(compiler, map->items[i][0]);  // key
    c_(compiler, map->items[i][1]);  // value
  }
  emit_byte(compiler, $BUILD_MAP, map->line);
  emit_byte(compiler, (byte_t)map->length, map->line);
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

void c_subscript(Compiler* compiler, AstNode* node) {
  c_(compiler, node->subscript.expr);
  c_(compiler, node->subscript.subscript);
  emit_byte(compiler, $SUBSCRIPT, node->subscript.line);
}

void c_expr_stmt(Compiler* compiler, AstNode* node) {
  c_(compiler, node->expr_stmt.expr);
  emit_byte(compiler, $POP, node->expr_stmt.line);
}

void c_show_stmt(Compiler* compiler, AstNode* node) {
  for (int i = node->show_stmt.length - 1; i >= 0; i--) {
    c_(compiler, node->show_stmt.items[i]);
  }
  emit_byte(compiler, $DISPLAY, node->show_stmt.line);
  emit_byte(compiler, (byte_t)node->show_stmt.length, node->show_stmt.line);
}

void c_assert_stmt(Compiler* compiler, AstNode* node) {
  c_(compiler, node->assert_stmt.msg);
  c_(compiler, node->assert_stmt.test);
  emit_byte(compiler, $ASSERT, node->assert_stmt.line);
}

void c_var(Compiler* compiler, AstNode* node) {
  VarNode* var = CAST(VarNode*, node);
  byte_t slot = store_variable(compiler, var);
  emit_byte(compiler, $GET_GLOBAL, var->line);
  emit_byte(compiler, slot, var->line);
}

void c_var_assign(Compiler* compiler, BinaryNode* assign) {
  // globals
  c_(compiler, assign->r_node);
  int slot = store_variable(compiler, &assign->l_node->var);
  emit_byte(compiler, $SET_GLOBAL, assign->line);
  emit_byte(compiler, (byte_t)slot, assign->line);
}

void c_subscript_assign(Compiler* compiler, BinaryNode* assign) {
  c_(compiler, assign->r_node);
  c_(compiler, assign->l_node);
  compiler->code->bytes[compiler->code->length - 1] = $SET_SUBSCRIPT;
}

void c_assign(Compiler* compiler, AstNode* node) {
  BinaryNode* assign = CAST(BinaryNode*, node);
  if (assign->l_node->num.type == AST_VAR) {
    c_var_assign(compiler, assign);
  } else if (assign->l_node->num.type == AST_SUBSCRIPT) {
    c_subscript_assign(compiler, assign);
  } else {
    compile_error(compiler, node, "Invalid assignment target");
  }
}

void c_var_decl(Compiler* compiler, AstNode* node) {
  BinaryNode* decl = CAST(BinaryNode*, node);
  byte_t slot = store_variable(compiler, &decl->l_node->var);
  c_(compiler, decl->r_node);
  emit_byte(compiler, $DEFINE_GLOBAL, decl->line);
  emit_byte(compiler, slot, decl->line);
}

void c_program(Compiler* compiler, AstNode* node) {
  ProgramNode* program = CAST(ProgramNode*, node);
  for (int i = 0; i < program->decls.len; i++) {
    c_(compiler, program->decls.items[i]);
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
    case AST_STR:
      c_str(compiler, node);
      break;
    case AST_LIST:
      c_list(compiler, node);
      break;
    case AST_MAP:
      c_map(compiler, node);
      break;
    case AST_SUBSCRIPT:
      c_subscript(compiler, node);
      break;
    case AST_EXPR_STMT:
      c_expr_stmt(compiler, node);
      break;
    case AST_SHOW_STMT:
      c_show_stmt(compiler, node);
      break;
    case AST_VAR_DECL:
      c_var_decl(compiler, node);
      break;
    case AST_VAR:
      c_var(compiler, node);
      break;
    case AST_ASSIGN:
      c_assign(compiler, node);
      break;
    case AST_ASSERT_STMT:
      c_assert_stmt(compiler, node);
      break;
    case AST_PROGRAM:
      c_program(compiler, node);
      break;
    default:
      UNREACHABLE("compile - unknown ast node");
  }
}

void compile(Compiler* compiler) {
  c_(compiler, compiler->root);
  emit_byte(compiler, $RET, 100);
}
#pragma clang diagnostic pop