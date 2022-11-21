#include "vm.h"

#define READ_BYTE(vm) (*(vm->fp->ip++))
#define READ_SHORT(vm) \
  (vm->fp->ip += 2, ((vm->fp->ip[-2]) << 8u) | (vm->fp->ip[-1]))
#define READ_CONST(vm) \
  (vm->fp->closure->func->code.vpool.values[READ_BYTE(vm)])
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

static IResult runtime_error(VM* vm, char* fmt, ...);
inline static void close_upvalues(VM* vm, const Value* slot);

inline static Value pop_stack(VM* vm) {
  return *(--vm->sp);
}

inline static void push_stack(VM* vm, Value val) {
  *vm->sp = val;
  vm->sp++;
}

Value vm_pop_stack(VM* vm) {
  return pop_stack(vm);
}

void vm_push_stack(VM* vm, Value val) {
  push_stack(vm, val);
}

inline static bool push_frame(VM* vm, CallFrame frame) {
  if (vm->frame_count >= FRAME_MAX) {
    runtime_error(vm, "Stack overflow: too many call frames");
    return false;
  }
  vm->fp = &vm->frames[vm->frame_count++];
  *vm->fp = frame;
  return true;
}

inline static CallFrame pop_frame(VM* vm) {
  // get last pushed frame
  CallFrame frame = vm->frames[vm->frame_count - 1];
  --vm->frame_count;
  if (vm->frame_count > 0) {
    // set current frame
    vm->fp = &vm->frames[vm->frame_count - 1];
  }
  return frame;
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
  VM vm = {
      .fp = NULL,
      .objects = NULL,
      .sp = NULL,
      .frame_count = 0,
      .is_compiling = true,
      .upvalues = NULL,
      .compiler = NULL};
  vm.sp = vm.stack;
  hashmap_init(&vm.strings);
  hashmap_init(&vm.globals);
  gc_init(&vm.gc);
  return vm;
}

void free_vm(VM* vm) {
  if (vm->globals.entries) {
    FREE_BUFFER(vm, vm->globals.entries, HashEntry, vm->globals.capacity);
  }
  if (vm->strings.entries) {
    FREE_BUFFER(vm, vm->strings.entries, HashEntry, vm->strings.capacity);
  }
  Obj* next;
  for (Obj* obj = vm->objects; obj != NULL; obj = next) {
    next = obj->next;
    free_object(vm, obj);
  }
  gc_free(&vm->gc);
}

void boot_vm(VM* vm, ObjFn* func) {
  vm->fp = vm->frames;
  vm->sp = vm->stack;
  // assumes that 'strings' hashmap has been initialized already by the compiler
  hashmap_init(&vm->globals);
  ObjClosure* closure = create_closure(vm, func);
  CallFrame frame = {
      .ip = func->code.bytes,
      .closure = closure,
      .stack = vm->sp};
  push_frame(vm, frame);
  push_stack(vm, OBJ_VAL(closure));
  vm->is_compiling = false;
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
      push_stack(vm, val);  // gc reasons
      push_stack(vm, subscript);  // gc reasons
      runtime_error(
          vm,
          "hashmap has no such key: '%s'",
          AS_STRING(value_to_string(vm, subscript))->str);
      vm->sp -= 2;  // gc reasons
    }
  } else if (IS_STRING(val)) {
    ObjString* str = AS_STRING(val);
    int64_t index;
    if (validate_subscript(vm, subscript, str->length, "string", &index)) {
      // gc reasons
      push_stack(vm, val);
      push_stack(vm, subscript);
      Value new_str =
          create_string(vm, &vm->strings, &str->str[index], 1, false);
      vm->sp -= 2;
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

static bool perform_prop_access(VM* vm, Value value, Value property) {
  ObjString* prop = AS_STRING(property);
  if (IS_STRUCT(value)) {
    ObjStruct* strukt = AS_STRUCT(value);
    Value res;
    if ((res = hashmap_get(&strukt->fields, property)) != NOTHING_VAL) {
      push_stack(vm, res);
      return true;
    } else {
      runtime_error(vm, "Illegal/unknown property access '%s'", prop->str);
    }
  } else if (IS_INSTANCE(value)) {
    ObjInstance* instance = AS_INSTANCE(value);
    Value res;
    if ((res = hashmap_get(&instance->fields, property)) != NOTHING_VAL) {
      push_stack(vm, res);
      return true;
    } else if ((hashmap_has_key(
                   &instance->strukt->fields,
                   property,
                   &res))) {
      /*
       * here, the property doesn't exist as a key in the instance's `fields` which could mean two things:
       * 1. the key wasn't assigned when creating the instance e.g. Bar {}; instead of Bar { a = something };
       * 2. the key is invalid. e.g. belongs to the struct, or isn't a field.
       * For (1.) we look up the key in the instance's struct, if the struct has such a key, and the key doesn't
       * belong to the struct (i.e. key is NOTHING_VAL), we can safely set it as `None` on the instance
       */
      if (res == NOTHING_VAL) {
        push_stack(vm, value);  // gc reasons
        push_stack(vm, property);  // gc reasons
        hashmap_put(&instance->fields, vm, property, NONE_VAL);
        vm->sp -= 2;  // gc reasons
        push_stack(vm, NONE_VAL);
        return true;
      }
    }
    runtime_error(vm, "Illegal/unknown property access '%s'", prop->str);
  } else {
    runtime_error(
        vm,
        "'%s' type has no property '%s'",
        get_value_type(value),
        prop->str);
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
  } else {
    runtime_error(
        vm,
        "'%s' type is not subscript-assignable",
        get_value_type(var));
  }
  return false;
}

inline static bool call_value(VM* vm, Value val, int argc, bool is_tco) {
  if (IS_CLOSURE(val)) {
    ObjFn* fn = AS_CLOSURE(val)->func;
    if (argc == fn->arity) {
      if (!is_tco) {
        CallFrame frame = {
            .closure = AS_CLOSURE(val),
            .stack = vm->sp - 1 - argc,
            .ip = fn->code.bytes};
        return push_frame(vm, frame);
      } else {
        // close all upvalues currently still unclosed
        close_upvalues(vm, vm->fp->stack);
        int j = 0;
        for (int i = argc; i >= 0; i--, j++) {
          vm->fp->stack[j] = PEEK_STACK_AT(vm, i);
        }
        vm->sp = vm->fp->stack + j;
        vm->fp->ip = fn->code.bytes;
        return true;
      }
    }
    runtime_error(
        vm,
        "'%s' function takes %d argument(s) but got %d",
        get_func_name(fn),
        fn->arity,
        argc);
  } else {
    runtime_error(vm, "'%s' type is not callable", get_value_type(val));
  }
  return false;
}

inline static ObjUpvalue* capture_upvalue(VM* vm, Value* position) {
  ObjUpvalue *current = vm->upvalues, *previous = NULL;
  while (current != NULL && current->location > position) {
    previous = current;
    current = current->next;
  }
  // if the location is already captured, return the upvalue
  if (current && current->location == position) {
    return current;
  }

  ObjUpvalue* new_uv = create_upvalue(vm, position);
  new_uv->next = current;  // enables sorting of the locations
  if (previous == NULL) {  // if head is empty
    vm->upvalues = new_uv;
  } else {
    /*
     * if all the captured locations are above 'position'
     * | p | x | x | x |
     * or if we passed 'position' during our search
     * | x | p | x | x |
     */
    previous->next = new_uv;
  }
  return new_uv;
}

inline static void close_upvalues(VM* vm, const Value* slot) {
  ObjUpvalue* current = vm->upvalues;
  while (current != NULL && current->location >= slot) {
    current->value = *current->location;
    current->location = &current->value;
    current = current->next;
  }
  vm->upvalues = current;
}

IResult run(VM* vm) {
  register byte_t inst;
  for (;;) {
#if defined(EVE_DEBUG_EXECUTION)
    print_stack(vm);
    dis_instruction(
        &vm->fp->closure->func->code,
        (int)(vm->fp->ip - vm->fp->closure->func->code.bytes));
#endif
    inst = READ_BYTE(vm);
    switch (inst) {
      case $LOAD_CONST: {
        push_stack(vm, READ_CONST(vm));
        continue;
      }
      case $DEFINE_GLOBAL: {
        Value var = READ_CONST(vm);
        hashmap_put(&vm->globals, vm, var, PEEK_STACK(vm));
        pop_stack(vm);
        continue;
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
        continue;
      }
      case $GET_LOCAL: {
        push_stack(vm, vm->fp->stack[READ_BYTE(vm)]);
        continue;
      }
      case $GET_UPVALUE: {
        push_stack(vm, *vm->fp->closure->env[READ_BYTE(vm)]->location);
        continue;
      }
      case $SET_GLOBAL: {
        Value var = READ_CONST(vm);
        if (hashmap_put(&vm->globals, vm, var, PEEK_STACK(vm))) {
          // true if key is new - new insertion, false if key already exists
          ObjString* str = AS_STRING(value_to_string(vm, var));
          hashmap_remove(&vm->globals, var);
          return runtime_error(
              vm,
              "use of undefined variable '%s'",
              str->str);
        }
        continue;
      }
      case $SET_LOCAL: {
        vm->fp->stack[READ_BYTE(vm)] = PEEK_STACK(vm);
        continue;
      }
      case $SET_UPVALUE: {
        *vm->fp->closure->env[READ_BYTE(vm)]->location = PEEK_STACK(vm);
        continue;
      }
      case $SET_SUBSCRIPT: {
        Value subscript = PEEK_STACK(vm);
        Value var = PEEK_STACK_AT(vm, 1);  // gc reasons
        Value value = PEEK_STACK_AT(vm, 2);  // gc reasons
        if (!perform_subscript_assign(vm, var, subscript, value)) {
          return RESULT_RUNTIME_ERROR;
        }
        pop_stack(vm);  // gc reasons
        pop_stack(vm);  // gc reasons
        continue;
      }
      case $RET: {
        CallFrame frame = pop_frame(vm);
        if (vm->frame_count == 0) {
          return RESULT_SUCCESS;
        }
        Value ret_val = pop_stack(vm);
        // close all upvalues currently still unclosed
        close_upvalues(vm, frame.stack);
        vm->sp = frame.stack;
        push_stack(vm, ret_val);
        continue;
      }
      case $TAIL_CALL:
      case $CALL: {
        int argc = READ_BYTE(vm);
        if (!call_value(
                vm,
                PEEK_STACK_AT(vm, argc),
                argc,
                inst == $TAIL_CALL)) {
          return RESULT_RUNTIME_ERROR;
        }
        continue;
      }
      case $ADD: {
        BINARY_OP(vm, +, NUMBER_VAL)
        continue;
      }
      case $SUBTRACT: {
        BINARY_OP(vm, -, NUMBER_VAL)
        continue;
      }
      case $DIVIDE: {
        BINARY_OP(vm, /, NUMBER_VAL)
        continue;
      }
      case $MULTIPLY: {
        BINARY_OP(vm, *, NUMBER_VAL)
        continue;
      }
      case $LESS: {
        BINARY_OP(vm, <, BOOL_VAL);
        continue;
      }
      case $GREATER: {
        BINARY_OP(vm, >, BOOL_VAL);
        continue;
      }
      case $LESS_OR_EQ: {
        BINARY_OP(vm, <=, BOOL_VAL);
        continue;
      }
      case $GREATER_OR_EQ: {
        BINARY_OP(vm, >=, BOOL_VAL);
        continue;
      }
      case $POW: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        BINARY_CHECK(vm, **, a, b, IS_NUMBER);
        double res = pow(AS_NUMBER(a), AS_NUMBER(b));
        push_stack(vm, NUMBER_VAL(res));
        continue;
      }
      case $MOD: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        BINARY_CHECK(vm, %, a, b, IS_NUMBER);
        double res = fmod(AS_NUMBER(a), AS_NUMBER(b));
        push_stack(vm, NUMBER_VAL(res));
        continue;
      }
      case $NOT: {
        Value v = pop_stack(vm);
        push_stack(vm, BOOL_VAL(value_falsy(v)));
        continue;
      }
      case $NEGATE: {
        Value v = pop_stack(vm);
        UNARY_CHECK(vm, -, v, IS_NUMBER);
        push_stack(vm, NUMBER_VAL(-AS_NUMBER(v)));
        continue;
      }
      case $BW_INVERT: {
        Value v = pop_stack(vm);
        UNARY_CHECK(vm, ~, v, IS_NUMBER);
        push_stack(vm, NUMBER_VAL((~(int64_t)AS_NUMBER(v))));
        continue;
      }
      case $EQ: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        push_stack(vm, BOOL_VAL(value_equal(a, b)));
        continue;
      }
      case $NOT_EQ: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        push_stack(vm, BOOL_VAL(!value_equal(a, b)));
        continue;
      }
      case $POP: {
        pop_stack(vm);
        continue;
      }
      case $POP_N: {
        vm->sp -= READ_BYTE(vm);
        continue;
      }
      case $SUBSCRIPT: {
        Value subscript = pop_stack(vm);
        Value val = pop_stack(vm);
        if (!perform_subscript(vm, val, subscript)) {
          return RESULT_RUNTIME_ERROR;
        }
        continue;
      }
      case $GET_PROPERTY: {
        Value property = READ_CONST(vm);
        Value value = pop_stack(vm);
        if (!perform_prop_access(vm, value, property)) {
          return RESULT_RUNTIME_ERROR;
        }
        continue;
      }
      case $SET_PROPERTY: {
        Value property = READ_CONST(vm);
        Value var = PEEK_STACK(vm);
        if (IS_INSTANCE(var)) {
          ObjInstance* instance = AS_INSTANCE(var);
          // true if 'property' is a new key
          if (hashmap_put(
                  &instance->fields,
                  vm,
                  property,
                  PEEK_STACK_AT(vm, 1))) {
            hashmap_remove(&instance->fields, property);
            return runtime_error(
                vm,
                "Instance of %s has no property '%s'",
                instance->strukt->name->str,
                AS_STRING(property)->str);
          }
          pop_stack(vm);
        } else {
          return runtime_error(
              vm,
              "Cannot set property on type '%s'",
              get_value_type(var));
        }
        continue;
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
        continue;
      }
      case $ASSERT: {
        Value test = PEEK_STACK(vm);  // gc reasons
        Value msg = PEEK_STACK_AT(vm, 1);  // gc reasons
        if (value_falsy(test)) {
          return runtime_error(
              vm,
              "Assertion Failed: %s",
              AS_STRING(value_to_string(vm, msg))->str);
        }
        pop_stack(vm);  // gc reasons
        pop_stack(vm);  // gc reasons
        continue;
      }
      case $JMP: {
        uint16_t offset = READ_SHORT(vm);
        vm->fp->ip += offset;
        continue;
      }
      case $JMP_FALSE: {
        uint16_t offset = READ_SHORT(vm);
        if (value_falsy(PEEK_STACK(vm))) {
          vm->fp->ip += offset;
        }
        continue;
      }
      case $JMP_FALSE_OR_POP: {
        uint16_t offset = READ_SHORT(vm);
        if (value_falsy(PEEK_STACK(vm))) {
          vm->fp->ip += offset;
        } else {
          pop_stack(vm);
        }
        continue;
      }
      case $LOOP: {
        uint16_t offset = READ_SHORT(vm);
        vm->fp->ip -= offset;
        continue;
      }
      case $BUILD_LIST: {
        ObjList* list = create_list(vm, READ_BYTE(vm));
        for (int i = 0; i < list->elems.length; i++) {
          list->elems.buffer[i] = PEEK_STACK_AT(vm, i);
        }
        vm->sp -= list->elems.length;
        push_stack(vm, OBJ_VAL(list));
        continue;
      }
      case $BUILD_MAP: {
        uint32_t len = READ_BYTE(vm) * 2;
        ObjHashMap* map = create_hashmap(vm);
        push_stack(vm, OBJ_VAL(map));  // gc reasons
        Value key, val;
        for (int i = 0; i < len; i += 2) {
          val = PEEK_STACK_AT(vm, i + 1);
          key = PEEK_STACK_AT(vm, i + 2);
          hashmap_put(map, vm, key, val);
        }
        vm->sp -= len + 1;  // +1 for map (gc reasons)
        push_stack(vm, OBJ_VAL(map));
        continue;
      }
      case $BUILD_CLOSURE: {
        ObjClosure* closure = create_closure(vm, AS_FUNC(READ_CONST(vm)));
        push_stack(vm, OBJ_VAL(closure));
        for (int i = 0; i < closure->env_len; i++) {
          byte_t index = READ_BYTE(vm);
          byte_t is_local = READ_BYTE(vm);
          if (is_local) {
            closure->env[i] = capture_upvalue(vm, vm->fp->stack + index);
          } else {
            closure->env[i] = vm->fp->closure->env[index];
          }
        }
        continue;
      }
      case $BUILD_STRUCT: {
        Value name = READ_CONST(vm);
        byte_t field_count = READ_BYTE(vm) * 2;  // k-v pairs
        ObjStruct* strukt = create_struct(vm, AS_STRING(name));
        push_stack(vm, OBJ_VAL(strukt));  // gc reasons
        Value var, val;
        for (byte_t i = 0; i < field_count; i += 2) {
          val = PEEK_STACK_AT(vm, i + 1);
          var = PEEK_STACK_AT(vm, i + 2);
          if (!hashmap_put(&strukt->fields, vm, var, val)) {
            return runtime_error(
                vm,
                "Duplicate field '%s'\n"
                "struct fields must be unique irrespective of meta-type",
                AS_STRING(value_to_string(vm, var))->str);
          }
        }
        vm->sp -= field_count + 1;  // +1 for strukt (gc reasons)
        push_stack(vm, OBJ_VAL(strukt));
        continue;
      }
      case $BUILD_INSTANCE: {
        Value var = PEEK_STACK(vm);  // gc reasons
        byte_t field_count = READ_BYTE(vm) * 2;  // k-v pairs
        if (!IS_STRUCT(var)) {
          return runtime_error(
              vm,
              "Cannot instantiate '%s' type",
              get_value_type(var));
        }
        Value key, val, check;
        ObjStruct* strukt = AS_STRUCT(var);
        ObjInstance* instance = create_instance(vm, strukt);
        push_stack(vm, OBJ_VAL(instance));  // gc reasons
        for (int i = 0; i < field_count; i += 2) {
          val = PEEK_STACK_AT(vm, i + 2);
          key = PEEK_STACK_AT(vm, i + 3);
          if (hashmap_has_key(&strukt->fields, key, &check)
              && check == NOTHING_VAL) {
            // only store fields with NOTHING_VAL value flag in the instance's field
            hashmap_put(&instance->fields, vm, key, val);
          } else {
            return runtime_error(
                vm,
                "Illegal/unknown instance property access '%s'",
                AS_STRING(key)->str);
          }
        }
        vm->sp -= field_count + 2;  // +2 for var and instance (gc reasons)
        push_stack(vm, OBJ_VAL(instance));
        continue;
      }
      case $CLOSE_UPVALUE: {
        close_upvalues(vm, vm->sp - 1);
        pop_stack(vm);
        continue;
      }
      case $BW_XOR: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        BINARY_CHECK(vm, ^, a, b, IS_NUMBER);
        push_stack(
            vm,
            NUMBER_VAL(((int64_t)AS_NUMBER(a) ^ (int64_t)AS_NUMBER(b))));
        continue;
      }
      case $BW_OR: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        BINARY_CHECK(vm, |, a, b, IS_NUMBER);
        push_stack(
            vm,
            NUMBER_VAL(((int64_t)AS_NUMBER(a) | (int64_t)AS_NUMBER(b))));
        continue;
      }
      case $BW_AND: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        BINARY_CHECK(vm, &, a, b, IS_NUMBER);
        push_stack(
            vm,
            NUMBER_VAL(((int64_t)AS_NUMBER(a) & (int64_t)AS_NUMBER(b))));
        continue;
      }
      case $BW_LSHIFT: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        BINARY_CHECK(vm, <<, a, b, IS_NUMBER);
        push_stack(
            vm,
            NUMBER_VAL(
                (double)((int64_t)AS_NUMBER(a) << (int64_t)AS_NUMBER(b))));
        continue;
      }
      case $BW_RSHIFT: {
        Value b = pop_stack(vm);
        Value a = pop_stack(vm);
        BINARY_CHECK(vm, >>, a, b, IS_NUMBER);
        push_stack(
            vm,
            NUMBER_VAL(
                (double)((int64_t)AS_NUMBER(a) >> (int64_t)AS_NUMBER(b))));
        continue;
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
