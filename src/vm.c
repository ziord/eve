#include "vm.h"

#define READ_BYTE(vm) (*(vm->ip++))
#define READ_SHORT(vm) (vm->ip += 2, ((vm->ip[-2]) << 8u) | (vm->ip[-1]))
#define READ_CONST(vm) (vm->code->vpool.values[READ_BYTE(vm)])
#define PEEK_STACK(vm) (*(vm->sp - 1))
#define PEEK_STACK_AT(vm, n) (*(vm->sp - 1 - (n)))
#define BINARY_OP(vm, _op, _val_func) \
  { \
    Value _r = pop_stack(vm); \
    Value _l = pop_stack(vm); \
    if (IS_NUMBER(_l) && IS_NUMBER(_r)) { \
      push_stack(vm, _val_func(AS_NUMBER(_l) _op AS_NUMBER(_r))); \
    } else { \
      return runtime_error( \
          vm, \
          "%s: %s and %s", \
          "Unsupported operand types for " \
          "'" #_op "'", \
          get_value_type(_l), \
          get_value_type(_r)); \
    } \
  }
#define BINARY_CHECK(vm, _op, _a, _b, check) \
  if (!check((_a)) || !check((_b))) { \
    return runtime_error( \
        vm, \
        "%s: %s and %s", \
        "Unsupported operand types for " \
        "'" #_op "'", \
        get_value_type(_a), \
        get_value_type(_b)); \
  }
#define UNARY_CHECK(vm, _op, _a, check) \
  if (!check((_a))) { \
    return runtime_error( \
        vm, \
        "%s: %s", \
        "Unsupported operand type for " \
        "'" #_op "'", \
        get_value_type((_a))); \
  }

inline static Value pop_stack(VM* vm) {
  return *(--vm->sp);
}

inline static void push_stack(VM* vm, Value val) {
  *vm->sp = val;
  vm->sp++;
}

inline static void print_stack(VM* vm) {
  printf("\t\t\t\t");
  for (Value* sp = vm->stack; sp < vm->sp; sp++) {
    printf("[ ");
    print_value(*sp);
    printf(" ]");
  }
  printf("\n");
}

VM new_vm() {
  VM vm = {.code = NULL, .ip = NULL, .sp = NULL};
  hashmap_init(&vm.strings);
  hashmap_init(&vm.globals);
  return vm;
}

void boot_vm(VM* vm, Code* code) {
  vm->code = code;
  vm->ip = vm->code->bytes;
  vm->sp = vm->stack;
  vm->objects = NULL;
  vm->bytes_alloc = 0;
  // assumes that 'strings' hashmap has been initialized already by the compiler
  hashmap_init(&vm->globals);
  // TODO: push function object instead of NOTHING
  push_stack(vm, NOTHING_VAL);
}

static IResult runtime_error(VM* vm, char* fmt, ...) {
  va_list ap;
  fputs("Runtime Error: ", stderr);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  return RESULT_RUNTIME_ERROR;
}

inline static bool validate_subscript(
    VM* vm,
    Value subscript,
    int max,
    char* type,
    int64_t* validated) {
  if (!IS_NUMBER(subscript)) {
    runtime_error(
        vm,
        "%s subscript must be an integer, not %s",
        type,
        get_value_type(subscript));
    return false;
  }
  double index = AS_NUMBER(subscript);
  if (index < 0) {
    index += max;
  }
  if (index > max || index < 0) {  // < 0 if len is 0
    runtime_error(vm, "%s index not in range", type);
    return false;
  } else if ((index != (int64_t)index)) {
    runtime_error(vm, "%s subscript must be an integer", type);
    return false;
  }
  *validated = (int64_t)index;
  return true;
}

static bool perform_subscript(VM* vm, Value val, Value subscript) {
  if (IS_LIST(val)) {
    ObjList* list = AS_LIST(val);
    int64_t index;
    if (validate_subscript(
            vm,
            subscript,
            list->elems.length,
            "list",
            &index)) {
      push_stack(vm, list->elems.buffer[index]);
      return true;
    }
  } else if (IS_HMAP(val)) {
    Value res = hashmap_get(AS_HMAP(val), subscript);
    if (res != NOTHING_VAL) {
      push_stack(vm, res);
      return true;
    } else {
      runtime_error(
          vm,
          "hashmap has no such key: '%s'",
          AS_STRING(value_to_string(vm, subscript))->str);
    }
  } else if (IS_STRING(val)) {
    ObjString* str = AS_STRING(val);
    int64_t index;
    if (validate_subscript(vm, subscript, str->length, "string", &index)) {
      Value new_str =
          create_string(vm, &vm->strings, &str->str[index], 1, false);
      push_stack(vm, new_str);
      return true;
    }
  } else {
    runtime_error(
        vm,
        "'%s' type is not subscriptable",
        get_value_type(val));
  }
  return false;
}

static bool
perform_subscript_assign(VM* vm, Value var, Value subscript, Value value) {
  if (IS_LIST(var)) {
    ObjList* list = AS_LIST(var);
    int64_t index;
    if (validate_subscript(
            vm,
            subscript,
            list->elems.length,
            "list",
            &index)) {
      list->elems.buffer[index] = value;
      return true;
    }
  } else if (IS_HMAP(var)) {
    hashmap_put(AS_HMAP(var), vm, subscript, value);
    return true;
  }
  runtime_error(
      vm,
      "'%s' type is not subscript-assignable",
      get_value_type(var));
  return false;
}

IResult run(VM* vm) {
  register byte_t inst;
  for (;;) {
#if defined(DEBUG_EXECUTION)
    print_stack(vm);
    dis_instruction(vm->code, (int)(vm->ip - vm->code->bytes));
#endif
    inst = READ_BYTE(vm);
    switch (inst) {
      case $LOAD_CONST: {
        push_stack(vm, READ_CONST(vm));
        break;
      }
      case $DEFINE_GLOBAL: {
        Value var = READ_CONST(vm);
        hashmap_put(&vm->globals, vm, var, pop_stack(vm));
        break;
      }
      case $GET_GLOBAL: {
        Value var = READ_CONST(vm);
        Value val;
        if ((val = hashmap_get(&vm->globals, var)) != NOTHING_VAL) {
          push_stack(vm, val);
        } else {
          return runtime_error(
              vm,
              "Name '%s' is not defined",
              AS_STRING(var)->str);
        }
        break;
      }
      case $GET_LOCAL: {
        push_stack(vm, vm->stack[READ_BYTE(vm)]);
        break;
      }
      case $SET_GLOBAL: {
        Value var = READ_CONST(vm);
        if (hashmap_put(&vm->globals, vm, var, PEEK_STACK(vm))) {
          // true if key is new - new insertion, false if key already exists
          hashmap_remove(&vm->globals, var);
          ObjString* str = AS_STRING(value_to_string(vm, var));
          return runtime_error(
              vm,
              "use of undefined variable '%s'",
              str->str);
        }
        break;
      }
      case $SET_LOCAL: {
        vm->stack[READ_BYTE(vm)] = PEEK_STACK(vm);
        break;
      }
      case $SET_SUBSCRIPT: {
        Value subscript = pop_stack(vm);
        Value var = pop_stack(vm);
        Value value = PEEK_STACK(vm);
        if (!perform_subscript_assign(vm, var, subscript, value)) {
          return RESULT_RUNTIME_ERROR;
        }
        break;
      }
      case $RET: {
        return RESULT_SUCCESS;
      }
      case $ADD: {
        BINARY_OP(vm, +, NUMBER_VAL)
        break;
      }
      case $SUBTRACT: {
        BINARY_OP(vm, -, NUMBER_VAL)
        break;
      }
      case $DIVIDE: {
        BINARY_OP(vm, /, NUMBER_VAL)
        break;
      }
      case $MULTIPLY: {
        BINARY_OP(vm, *, NUMBER_VAL)
        break;
      }
      case $LESS: {
        BINARY_OP(vm, <, BOOL_VAL);
        break;
      }
      case $GREATER: {
        BINARY_OP(vm, >, BOOL_VAL);
        break;
      }
      case $LESS_OR_EQ: {
        BINARY_OP(vm, <=, BOOL_VAL);
        break;
      }
      case $GREATER_OR_EQ: {
        BINARY_OP(vm, >=, BOOL_VAL);
        break;
      }
      case $POW: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        BINARY_CHECK(vm, **, a, b, IS_NUMBER);
        double res = pow(AS_NUMBER(a), AS_NUMBER(b));
        push_stack(vm, NUMBER_VAL(res));
        break;
      }
      case $MOD: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        BINARY_CHECK(vm, %, a, b, IS_NUMBER);
        double res = fmod(AS_NUMBER(a), AS_NUMBER(b));
        push_stack(vm, NUMBER_VAL(res));
        break;
      }
      case $NOT: {
        Value v = pop_stack(vm);
        push_stack(vm, BOOL_VAL(value_falsy(v)));
        break;
      }
      case $NEGATE: {
        Value v = pop_stack(vm);
        UNARY_CHECK(vm, -, v, IS_NUMBER);
        push_stack(vm, NUMBER_VAL(-AS_NUMBER(v)));
        break;
      }
      case $BW_INVERT: {
        Value v = pop_stack(vm);
        UNARY_CHECK(vm, ~, v, IS_NUMBER);
        push_stack(vm, NUMBER_VAL((~(int64_t)AS_NUMBER(v))));
        break;
      }
      case $EQ: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        push_stack(vm, BOOL_VAL(value_equal(a, b)));
        break;
      }
      case $NOT_EQ: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        push_stack(vm, BOOL_VAL(!value_equal(a, b)));
        break;
      }
      case $POP: {
        pop_stack(vm);
        break;
      }
      case $POP_N: {
        vm->sp -= READ_BYTE(vm);
        break;
      }
      case $SUBSCRIPT: {
        Value subscript = pop_stack(vm);
        Value val = pop_stack(vm);
        if (!perform_subscript(vm, val, subscript)) {
          return RESULT_RUNTIME_ERROR;
        }
        break;
      }
      case $DISPLAY: {  // TODO: remove
        byte_t len = READ_BYTE(vm);
        for (int i = 0; i < len; i++) {
          print_value(PEEK_STACK_AT(vm, i));
          if (i < len - 1) {
            printf(" ");
          }
        }
        printf("\n");
        vm->sp -= len;
        break;
      }
      case $ASSERT: {
        Value test = pop_stack(vm);
        Value msg = pop_stack(vm);
        if (value_falsy(test)) {
          return runtime_error(
              vm,
              "Assertion Failed: %s",
              AS_STRING(value_to_string(vm, msg))->str);
        }
        break;
      }
      case $JMP: {
        uint16_t offset = READ_SHORT(vm);
        vm->ip += offset;
        break;
      }
      case $JMP_FALSE: {
        uint16_t offset = READ_SHORT(vm);
        if (value_falsy(PEEK_STACK(vm))) {
          vm->ip += offset;
        }
        break;
      }
      case $JMP_FALSE_OR_POP: {
        uint16_t offset = READ_SHORT(vm);
        if (value_falsy(PEEK_STACK(vm))) {
          vm->ip += offset;
        } else {
          pop_stack(vm);
        }
        break;
      }
      case $BUILD_LIST: {
        ObjList* list = create_list(vm, READ_BYTE(vm));
        for (int i = 0; i < list->elems.length; i++) {
          list->elems.buffer[i] = PEEK_STACK_AT(vm, i);
        }
        vm->sp -= list->elems.length;
        push_stack(vm, OBJ_VAL(list));
        break;
      }
      case $BUILD_MAP: {
        uint32_t len = READ_BYTE(vm) * 2;
        ObjHashMap* map = create_hashmap(vm);
        Value key, val;
        for (int i = 0; i < len; i += 2) {
          val = PEEK_STACK_AT(vm, i);
          key = PEEK_STACK_AT(vm, i + 1);
          hashmap_put(map, vm, key, val);
        }
        vm->sp -= len;
        push_stack(vm, OBJ_VAL(map));
        break;
      }
      case $BW_XOR: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        BINARY_CHECK(vm, ^, a, b, IS_NUMBER);
        push_stack(
            vm,
            NUMBER_VAL(((int64_t)AS_NUMBER(a) ^ (int64_t)AS_NUMBER(b))));
        break;
      }
      case $BW_OR: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        BINARY_CHECK(vm, |, a, b, IS_NUMBER);
        push_stack(
            vm,
            NUMBER_VAL(((int64_t)AS_NUMBER(a) | (int64_t)AS_NUMBER(b))));
        break;
      }
      case $BW_AND: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        BINARY_CHECK(vm, &, a, b, IS_NUMBER);
        push_stack(
            vm,
            NUMBER_VAL(((int64_t)AS_NUMBER(a) & (int64_t)AS_NUMBER(b))));
        break;
      }
      case $BW_LSHIFT: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        BINARY_CHECK(vm, <<, a, b, IS_NUMBER);
        push_stack(
            vm,
            NUMBER_VAL(
                (double)((int64_t)AS_NUMBER(a) << (int64_t)AS_NUMBER(b))));
        break;
      }
      case $BW_RSHIFT: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        BINARY_CHECK(vm, >>, a, b, IS_NUMBER);
        push_stack(
            vm,
            NUMBER_VAL(
                (double)((int64_t)AS_NUMBER(a) >> (int64_t)AS_NUMBER(b))));
        break;
      }
      default:
        UNREACHABLE("unknown opcode");
    }
  }
}

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONST
#undef PEEK_STACK
#undef PEEK_STACK_AT
#undef BINARY_OP
#undef BINARY_CHECK
#undef UNARY_CHECK
