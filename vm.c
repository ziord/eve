#include "vm.h"

#define READ_BYTE(vm) (*(vm->ip++))
#define READ_CONST(vm) (vm->code->vpool.values[READ_BYTE(vm)])
#define BINARY_OP(vm, _op) \
  { \
    Value _r = pop_stack(vm); \
    Value _l = pop_stack(vm); \
    if (IS_NUMBER(_l) && IS_NUMBER(_r)) { \
      push_stack(vm, NUMBER_VAL(AS_NUMBER(_l) _op AS_NUMBER(_r))); \
    } else { \
      return runtime_error( \
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
        "%s: %s and %s", \
        "Unsupported operand types for " \
        "'" #_op "'", \
        get_value_type(_a), \
        get_value_type(_b)); \
  }
#define UNARY_CHECK(vm, _op, _a, check) \
  if (!check((_a))) { \
    return runtime_error( \
        "%s: %s and %s", \
        "Unsupported operand type for " \
        "'" #_op "'", \
        get_value_type(_a)); \
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

static IResult runtime_error(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fputs("Runtime Error: ", stderr);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  return RESULT_RUNTIME_ERROR;
}

VM new_vm() {
  VM vm = {.code = NULL, .ip = NULL, .sp = NULL};
  return vm;
}

void init_vm(VM* vm, Code* code) {
  vm->code = code;
  vm->ip = vm->code->bytes;
  vm->sp = vm->stack;
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
      case $RET: {
        printf("\n");
        print_value(pop_stack(vm));
        printf("\n");
        return RESULT_SUCCESS;
      }
      case $ADD: {
        BINARY_OP(vm, +)
        break;
      }
      case $SUBTRACT: {
        BINARY_OP(vm, -)
        break;
      }
      case $DIVIDE: {
        BINARY_OP(vm, /)
        break;
      }
      case $MULTIPLY: {
        BINARY_OP(vm, *)
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
        push_stack(vm, BOOL_VAL(!is_falsey_value(v)));
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
        push_stack(vm, NUMBER_VAL((~(uint64_t)AS_NUMBER(v))));
        break;
      }
      default:
        UNREACHABLE("unknown opcode");
    }
  }
}

#undef READ_BYTE
#undef READ_CONST
#undef BINARY_OP