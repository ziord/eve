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
    [OP_DCOL] = {0xff, 0xff},
    [OP_DOT] = {0xff, 0xff},
    [OP_EQ] = {0xff, 0xff},
    [OP_OR] = {0xff, 0xff},
    [OP_AND] = {0xff, 0xff},
};

void c_(Compiler* compiler, AstNode* node);
static void reserve_local(Compiler* compiler);
void c_control(
    Compiler* compiler,
    ControlStmtNode* node,
    int continue_exit,
    int break_exit);

void new_compiler(
    Compiler* compiler,
    AstNode* node,
    ObjFn* func,
    VM* vm,
    char* module_name) {
  *compiler = (Compiler) {
      .root = node,
      .func = func,
      .vm = vm,
      .scope = 0,
      .locals_count = 0,
      .controls_count = 0,
      .upvalues_count = 0,
      .errors = 0,
      .current_loop = {.scope = 0},
      .enclosing = NULL};
  if (module_name) {
    Value name = create_string(
        vm,
        &vm->strings,
        module_name,
        (int)strlen(module_name),
        false);
    compiler->module = create_module(vm, AS_STRING(name));
    compiler->func->module = compiler->module;
  }
  if (!vm->compiler) {
    vm->compiler = compiler;
  }
  reserve_local(compiler);
}

void compile_error(Compiler* compiler, char* fmt, ...) {
  if (compiler->errors > 1) {
    fputc('\n', stderr);
  }
  compiler->errors++;
  va_list ap;
  fputs("Compile Error: ", stderr);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

static int store_variable(Compiler* compiler, VarNode* var) {
  Value val = create_string(
      compiler->vm,
      &compiler->vm->strings,
      var->name,
      var->len,
      false);
  int slot = write_value(&compiler->func->code.vpool, val, compiler->vm);
  if (slot >= CONST_MAX) {
    compile_error(compiler, "Too many constants");
  }
  return slot;
}

static void load_variable(Compiler* compiler, VarNode* var, byte_t op) {
  byte_t slot = store_variable(compiler, var);
  emit_byte(compiler, op, var->line);
  emit_byte(compiler, slot, var->line);
}

inline static void reserve_local(Compiler* compiler) {
  LocalVar* local_var = &compiler->locals[compiler->locals_count++];
  local_var->name_len = 0;
  local_var->name = "";
  local_var->initialized = false;
  local_var->scope = compiler->scope;
}

inline static int add_lvar(Compiler* compiler, VarNode* node) {
  LocalVar* var = &compiler->locals[compiler->locals_count++];
  var->scope = compiler->scope;
  var->name = node->name;
  var->name_len = node->len;
  var->initialized = false;
  return compiler->locals_count - 1;
}

inline static int
add_upvalue(Compiler* compiler, int index, bool is_local) {
  if (compiler->upvalues_count >= CONST_MAX) {
    compile_error(
        compiler,
        "Too many closure captures. Max allowed is: %d",
        CONST_MAX);
    return -1;
  }
  Upvalue* upvalue;
  // ensure we do not add duplicate entries for the same upvalue, to preserve space
  for (int i = 0; i < compiler->upvalues_count; i++) {
    upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->is_local == is_local) {
      return i;
    }
  }
  compiler->upvalues[compiler->upvalues_count] =
      (Upvalue) {.index = index, .is_local = is_local};
  return compiler->upvalues_count++;
}

inline static int init_lvar(Compiler* compiler, VarNode* node) {
  int index = add_lvar(compiler, node);
  compiler->locals[index].initialized = true;
  return index;
}

inline static int find_lvar(Compiler* compiler, VarNode* node) {
  LocalVar* var;
  // we start at the latest lvar - to ensure lexical scoping correctness
  for (int i = compiler->locals_count - 1; i >= 0; i--) {
    var = &compiler->locals[i];
    if (compiler->scope < var->scope) {
      // prevent access to inner scopes, i.e. scopes deeper
      // than the compiler's current scope
      break;
    } else if (
        var->name_len == node->len
        && memcmp(var->name, node->name, node->len) == 0) {
      return i;
    }
  }
  return -1;
}

inline static int find_upvalue(Compiler* compiler, VarNode* node) {
  if (!compiler->enclosing) {
    return -1;
  }
  int index = find_lvar(compiler->enclosing, node);
  if (index != -1) {
    compiler->enclosing->locals[index].is_captured = true;
    index = add_upvalue(compiler, index, true);
  } else {
    // find upvalues in higher nested functions, and propagate downwards
    index = find_upvalue(compiler->enclosing, node);
    if (index != -1) {
      index = add_upvalue(compiler, index, false);
    }
  }
  return index;
}

inline static void pop_locals(Compiler* compiler, int line) {
  // TODO: 'pop' optimizations
  LocalVar* lvar;
  for (int i = compiler->locals_count - 1; i >= 0; i--) {
    lvar = &compiler->locals[i];
    if (lvar->scope > compiler->scope) {
      if (lvar->is_captured) {
        emit_byte(compiler, $CLOSE_UPVALUE, line);
      } else {
        emit_byte(compiler, $POP, line);
      }
      compiler->locals_count--;
    }
  }
}

void push_loop_ctrl(Compiler* compiler, ControlStmtNode* node) {
  ASSERT_MAX(
      compiler,
      compiler->controls_count,
      MAX_CONTROLS,
      "Maximum number of break/continue exceeded: %d",
      MAX_CONTROLS);
  compiler->controls[compiler->controls_count++] =
      (LoopVar) {.scope = compiler->current_loop.scope, .node = node};
}

void process_loop_control(
    Compiler* compiler,
    int continue_exit,
    int break_exit) {
  LoopVar* var;
  int j = 0;
  for (int i = compiler->controls_count - 1; i >= 0; i--) {
    var = &compiler->controls[i];
    if (var->scope == compiler->current_loop.scope) {
      c_control(compiler, var->node, continue_exit, break_exit);
      j++;
    }
  }
  compiler->controls_count -= j;
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
  emit_byte(compiler, $POP, node->line);
  c_(compiler, node->r_node);
  patch_jump(compiler, end_jmp_idx);
}

void c_dcol_dot(Compiler* compiler, BinaryNode* node) {
  // compiles expr::var (struct property access) or
  // expr.var (instance property access) i.e. OP_DCOL | OP_DOT
  c_(compiler, node->l_node);
  load_variable(
      compiler,
      &node->r_node->var,
      node->op == OP_DOT ? $GET_PROPERTY : $GET_FIELD);
}

void c_binary(Compiler* compiler, AstNode* node) {
  BinaryNode* bin = CAST(BinaryNode*, node);
  if (bin->op == OP_OR) {
    c_or(compiler, bin);
  } else if (bin->op == OP_AND) {
    c_and(compiler, bin);
  } else if (bin->op == OP_DCOL || bin->op == OP_DOT) {
    c_dcol_dot(compiler, bin);
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
  // firstly, check if it's a reference to a local var
  int index;
  if ((index = find_lvar(compiler, var)) != -1) {
    // check if the local is initialized, i.e. prevent things like: let a = a;
    LocalVar* lvar = &compiler->locals[index];
    if (!lvar->initialized) {
      return compile_error(
          compiler,
          "Cannot reference local variable '%.*s' before assignment",
          lvar->name_len,
          lvar->name);
    }
    emit_byte(compiler, $GET_LOCAL, var->line);
    emit_byte(compiler, (byte_t)index, var->line);
  } else if ((index = find_upvalue(compiler, var)) != -1) {
    emit_byte(compiler, $GET_UPVALUE, var->line);
    emit_byte(compiler, (byte_t)index, var->line);
  } else {
    load_variable(compiler, var, $GET_GLOBAL);
  }
}

void c_var_assign(Compiler* compiler, BinaryNode* assign) {
  c_(compiler, assign->r_node);
  int slot = find_lvar(compiler, &assign->l_node->var);
  if (slot != -1) {
    emit_byte(compiler, $SET_LOCAL, assign->line);
    emit_byte(compiler, (byte_t)slot, assign->line);
  } else if ((slot = find_upvalue(compiler, &assign->l_node->var)) != -1) {
    emit_byte(compiler, $SET_UPVALUE, assign->line);
    emit_byte(compiler, (byte_t)slot, assign->line);
  } else {
    // globals
    load_variable(compiler, &assign->l_node->var, $SET_GLOBAL);
  }
}

void c_subscript_assign(Compiler* compiler, BinaryNode* assign) {
  c_(compiler, assign->r_node);
  c_(compiler, assign->l_node);
  compiler->func->code.bytes[compiler->func->code.length - 1] =
      $SET_SUBSCRIPT;
}

void c_dot_assign(Compiler* compiler, BinaryNode* assign) {
  c_(compiler, assign->r_node);
  // change the ast node type from DOT_EXPR to BINARY, so that it invokes
  // c_binary() and in turn invokes c_dcol_dot()
  assign->l_node->num.type = AST_BINARY;
  c_(compiler, assign->l_node);
  // now rewrite $GET_PROPERTY emitted by c_dcol_dot() to $SET_PROPERTY
  compiler->func->code.bytes[compiler->func->code.length - 2] =
      $SET_PROPERTY;
}

void c_assign(Compiler* compiler, AstNode* node) {
  BinaryNode* assign = CAST(BinaryNode*, node);
  if (assign->l_node->num.type == AST_VAR) {
    c_var_assign(compiler, assign);
  } else if (assign->l_node->num.type == AST_SUBSCRIPT) {
    c_subscript_assign(compiler, assign);
  } else if (assign->l_node->num.type == AST_DOT_EXPR) {
    c_dot_assign(compiler, assign);
  } else {
    return compile_error(compiler, "Invalid assignment target");
  }
}

void c_var_decl(Compiler* compiler, AstNode* node) {
  BinaryNode* decl = CAST(BinaryNode*, node);
  if (compiler->scope > 0) {
    int index = add_lvar(compiler, &decl->l_node->var);
    c_(compiler, decl->r_node);
    compiler->locals[index].initialized = true;
  } else {
    c_(compiler, decl->r_node);
    load_variable(compiler, &decl->l_node->var, $DEFINE_GLOBAL);
  }
}

void c_block_stmt(Compiler* compiler, AstNode* node) {
  BlockStmtNode* block = CAST(BlockStmtNode*, node);
  compiler->scope++;
  for (int i = 0; i < vec_size(&block->stmts); i++) {
    c_(compiler, block->stmts.items[i]);
  }
  compiler->scope--;
  // use the last node's line if available, else the block's line
  int line = last_line(compiler);
  pop_locals(compiler, line != -1 ? line : block->line);
}

void c_if_stmt(Compiler* compiler, AstNode* node) {
  IfElseStmtNode* ife = CAST(IfElseStmtNode*, node);
  c_(compiler, ife->condition);
  int else_slot = emit_jump(compiler, $JMP_FALSE_OR_POP, ife->line);
  c_(compiler, ife->if_block);
  int end_slot = emit_jump(compiler, $JMP, last_line(compiler));
  patch_jump(compiler, else_slot);
  // pop if-condition when in else block
  emit_byte(compiler, $POP, last_line(compiler));
  c_(compiler, ife->else_block);
  patch_jump(compiler, end_slot);
}

void c_control(
    Compiler* compiler,
    ControlStmtNode* node,
    int continue_exit,
    int break_exit) {
  Code* code = &compiler->func->code;
  int slot = node->patch_slot;
  int offset;
  if (node->is_break) {
    offset = (break_exit - slot - 3);
    code->bytes[slot] = $JMP;
    code->bytes[slot + 1] = (offset >> 8) & 0xff;
    code->bytes[slot + 2] = offset & 0xff;
  } else {
    offset = (slot - continue_exit + 3);
    code->bytes[slot] = $LOOP;
    code->bytes[slot + 1] = (offset >> 8) & 0xff;
    code->bytes[slot + 2] = offset & 0xff;
  }
}

void c_control_stmt(Compiler* compiler, AstNode* node) {
  /*
   * <-> continue_point
   * while false {
   *   break;
   *   continue;
   * }
   * <-> break_point
   */
  LocalVar* lvar;
  int line = node->control_stmt.line;
  for (int i = compiler->locals_count - 1; i >= 0; i--) {
    lvar = &compiler->locals[i];
    // pop locals (if any) before exiting the loop via break or continue
    if (lvar->scope > compiler->current_loop.scope) {
      if (lvar->is_captured) {
        // also, handle upvalues, that is, ensure they're
        // converted/closed before going off the stack
        emit_byte(compiler, $CLOSE_UPVALUE, line);
      } else {
        emit_byte(compiler, $POP, line);
      }
    }
  }
  // use the next slot offset which would later contain the jump instruction
  node->control_stmt.patch_slot = compiler->func->code.length;
  emit_jump(compiler, 0xff, line);  // fill with a fake instruction for now
  push_loop_ctrl(compiler, &node->control_stmt);
}

void c_while_stmt(Compiler* compiler, AstNode* node) {
  WhileStmtNode* wh_node = CAST(WhileStmtNode*, node);
  LoopVar curr = compiler->current_loop;
  compiler->current_loop = (LoopVar) {.scope = compiler->scope};
  int continue_exit = compiler->func->code.length;
  c_(compiler, wh_node->condition);
  int end_slot = emit_jump(compiler, $JMP_FALSE_OR_POP, wh_node->line);
  c_(compiler, wh_node->block);
  emit_loop(compiler, continue_exit, last_line(compiler));
  patch_jump(compiler, end_slot);
  // pop loop condition off the stack
  emit_byte(compiler, $POP, last_line(compiler));
  int break_exit = compiler->func->code.length;
  process_loop_control(compiler, continue_exit, break_exit);
  compiler->current_loop = curr;
}

void c_return(Compiler* compiler, AstNode* node) {
  ExprStmtNode* expr = &node->expr_stmt;
  c_(compiler, expr->expr);
  emit_byte(compiler, $RET, expr->line);
}

void c_call(Compiler* compiler, AstNode* node) {
  CallNode* call_node = &node->call;
  c_(compiler, call_node->left);
  for (int i = 0; i < call_node->args_count; i++) {
    c_(compiler, call_node->args[i]);
  }
  emit_byte(
      compiler,
      call_node->is_tail_call ? $TAIL_CALL : $CALL,
      call_node->line);
  emit_byte(compiler, CAST(byte_t, call_node->args_count), call_node->line);
}

void c_struct(Compiler* compiler, AstNode* node) {
  StructNode* struct_n = &node->strukt;
  /*
   * struct Foo {
   *  @declare x => 5, z => 10;
   *  @compose x, z;
   *  @compose p;
   * }
   * push var
   * push val
   * BUILD_STRUCT name-slot field-count
   */
  StructMeta* meta;
  VarNode var;
  byte_t slot;
  for (int i = 0; i < struct_n->field_count; i++) {
    meta = &struct_n->fields[i];
    // compile var
    var = (VarNode) {
        .type = AST_VAR,
        .name = meta->var.value,
        .len = meta->var.length,
        .line = meta->var.line};
    load_variable(compiler, &var, $LOAD_CONST);
    if (meta->expr) {
      c_(compiler, meta->expr);
    } else {
      // use NOTHING as placeholder value
      emit_value(compiler, $LOAD_CONST, NOTHING_VAL, last_line(compiler));
    }
  }
  load_variable(compiler, &struct_n->name->var, $BUILD_STRUCT);
  emit_byte(compiler, (byte_t)struct_n->field_count, last_line(compiler));
}

void c_struct_call(Compiler* compiler, AstNode* node) {
  StructCallNode* struct_c = &node->struct_call;
  MapNode* map = &struct_c->fields;
  for (int i = map->length - 1; i >= 0; i--) {
    load_variable(compiler, &map->items[i][0]->var, $LOAD_CONST);  // key
    c_(compiler, map->items[i][1]);  // value
  }
  c_(compiler, struct_c->name);
  emit_byte(compiler, $BUILD_INSTANCE, last_line(compiler));
  emit_byte(compiler, (byte_t)map->length, last_line(compiler));
}

void c_function(Compiler* compiler, AstNode* node) {
  // let func = fn () {..} | fn func() {...}
  FuncNode* func = &node->func;
  // create a new compiler for the function
  ObjFn* fn_obj = create_function(compiler->vm);
  // only emit definition for globals
  bool emit_name = compiler->scope <= 0;
  int name_slot = -1;
  if (emit_name) {
    // global function
    if (func->name) {
      name_slot = store_variable(compiler, &func->name->var);
      fn_obj->name =
          AS_STRING(compiler->func->code.vpool.values[name_slot]);
    }
  } else {
    // local function
    // set as initialized for functions with recursive calls
    if (func->name) {
      init_lvar(compiler, &func->name->var);
      fn_obj->name = AS_STRING(create_string(
          compiler->vm,
          &compiler->vm->strings,
          func->name->var.name,
          func->name->var.len,
          false));
    }
  }
  Compiler func_compiler;
  new_compiler(&func_compiler, node, fn_obj, compiler->vm, NULL);
  fn_obj->module = func_compiler.module = compiler->module;  // set module
  func_compiler.enclosing = compiler;
  // compile params
  func_compiler.scope++;  // make params local to the function
  for (int i = 0; i < func->params_count; i++) {
    VarNode* param = &func->params[i]->var;
    init_lvar(&func_compiler, param);
  }
  // compile function body
  c_(&func_compiler, func->body);
  fn_obj->arity = func->params_count;
  fn_obj->env_len = func_compiler.upvalues_count;
  emit_value(compiler, $BUILD_CLOSURE, OBJ_VAL(fn_obj), func->line);
  // compile upvalues
  Upvalue* upvalue;
  for (int i = 0; i < func_compiler.upvalues_count; i++) {
    // emit upvalue-index, upvalue-is_local
    upvalue = &func_compiler.upvalues[i];
    emit_byte(compiler, (byte_t)upvalue->index, last_line(compiler));
    emit_byte(compiler, (byte_t)upvalue->is_local, last_line(compiler));
  }
  if (emit_name && name_slot != -1) {
    emit_byte(compiler, $DEFINE_GLOBAL, func->line);
    emit_byte(compiler, name_slot, func->line);
  }
#ifdef EVE_DEBUG
  if (!func_compiler.errors) {
    dis_code(&fn_obj->code, get_func_name(fn_obj));
    printf("\n");
  }
#endif
}

void c_program(Compiler* compiler, AstNode* node) {
  ProgramNode* program = CAST(ProgramNode*, node);
  for (int i = 0; i < vec_size(&program->decls); i++) {
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
    case AST_BLOCK_STMT:
      c_block_stmt(compiler, node);
      break;
    case AST_IF_STMT:
      c_if_stmt(compiler, node);
      break;
    case AST_WHILE_STMT:
      c_while_stmt(compiler, node);
      break;
    case AST_CONTROL_STMT:
      c_control_stmt(compiler, node);
      break;
    case AST_FUNC:
      c_function(compiler, node);
      break;
    case AST_RETURN_STMT:
      c_return(compiler, node);
      break;
    case AST_CALL:
      c_call(compiler, node);
      break;
    case AST_STRUCT:
      c_struct(compiler, node);
      break;
    case AST_STRUCT_CALL:
      c_struct_call(compiler, node);
      break;
    case AST_DOT_EXPR:
      c_binary(compiler, node);
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
  if (compiler->errors) {
    free_code(&compiler->func->code, compiler->vm);
  }
  emit_byte(compiler, $RET, last_line(compiler));
#ifdef EVE_DEBUG
  dis_code(&compiler->func->code, get_func_name(compiler->func));
#endif
}
#pragma clang diagnostic pop