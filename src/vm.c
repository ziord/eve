#include "vm.h"

#include "core.h"
#include "map.h"

#define READ_BYTE(vm) (*(vm->fp->ip++))
#define READ_SHORT(vm) \
  (vm->fp->ip += 2, ((vm->fp->ip[-2]) << 8u) | (vm->fp->ip[-1]))
#define READ_CONST(vm) (vm->fp->closure->func->code.vpool.values[READ_BYTE(vm)])
#define READ_STRING(vm) AS_STRING(READ_CONST(vm))
#define PEEK_STACK(vm) (*(vm->sp - 1))
#define PEEK_STACK_AT(vm, n) (*(vm->sp - 1 - (n)))
#define TRY_RECOVER(_vm_) \
  if (_vm_->has_error) \
    return RESULT_RUNTIME_ERROR; \
  else \
    DISPATCH();
#define BINARY_OP(vm, _op, _val_func) \
  { \
    Value _r = pop_stack(vm); \
    Value _l = pop_stack(vm); \
    if (IS_NUMBER(_l) && IS_NUMBER(_r)) { \
      push_stack(vm, _val_func(AS_NUMBER(_l) _op AS_NUMBER(_r))); \
    } else { \
      runtime_error( \
          vm, \
          NOTHING_VAL, \
          "%s: %s and %s", \
          "Unsupported operand types for " \
          "'" #_op "'", \
          get_value_type(_l), \
          get_value_type(_r)); \
      TRY_RECOVER(vm) \
    } \
  }
#define BINARY_CHECK(vm, _op, _a, _b, check) \
  if (!check((_a)) || !check((_b))) { \
    runtime_error( \
        vm, \
        NOTHING_VAL, \
        "%s: %s and %s", \
        "Unsupported operand types for " \
        "'" #_op "'", \
        get_value_type(_a), \
        get_value_type(_b)); \
    TRY_RECOVER(vm) \
  }
#define UNARY_CHECK(vm, _op, _a, check) \
  if (!check((_a))) { \
    runtime_error( \
        vm, \
        NOTHING_VAL, \
        "%s: %s", \
        "Unsupported operand type for " \
        "'" #_op "'", \
        get_value_type((_a))); \
    TRY_RECOVER(vm) \
  }
#define VM_LOOP \
  vm_loop: \
  switch ((inst = READ_BYTE(vm)))
#define DISPATCH() goto vm_loop
#define END_BRACE
#define KB_SIZE (1024)
#define pop_stack(vm) (*(--(vm)->sp))
#define push_stack(vm, val) ((*(vm)->sp = (val)), (vm)->sp++)
#define value_falsy(v) \
  (IS_NUMBER((v)) && !AS_NUMBER((v)) || (IS_BOOL((v)) && !AS_BOOL((v))) \
   || IS_NONE((v)) || (IS_LIST((v)) && !AS_LIST((v))->elems.length) \
   || (IS_STRING((v)) && !AS_STRING((v))->length) \
   || (IS_HMAP((v)) && !AS_HMAP((v))->length))

inline static void close_upvalues(VM* vm, const Value* slot);
inline static bool call_value(VM* vm, Value val, int argc, bool is_tco);

inline static TryCtx new_tryctx() {
  return (TryCtx) {.sp = NULL, .handler_ip = NULL};
}

Value vm_pop_stack(VM* vm) {
  return pop_stack(vm);
}

void vm_push_stack(VM* vm, Value val) {
  push_stack(vm, val);
}

bool vm_call_value(VM* vm, Value val, int argc) {
  return call_value(vm, val, argc, false);
}

inline static bool push_frame(VM* vm, CallFrame frame) {
  if (vm->frame_count >= CALL_FRAME_MAX) {
    runtime_error(vm, NOTHING_VAL, "Stack overflow: too many call frames");
    return false;
  }
  vm->fp = &vm->frames[vm->frame_count++];
  *vm->fp = frame;
  // set the current module
  vm->current_module = vm->fp->closure->func->module;
  return true;
}

inline static CallFrame pop_frame(VM* vm) {
  // get last pushed frame
  CallFrame frame = vm->frames[vm->frame_count - 1];
  --vm->frame_count;
  if (vm->frame_count > 0) {
    // set current frame
    vm->fp = &vm->frames[vm->frame_count - 1];
    // set the current module
    vm->current_module = vm->fp->closure->func->module;
  }
  return frame;
}

inline static bool set_try(VM* vm, int handler_ip) {
  TryCtx ctx = {.handler_ip = vm->fp->ip + handler_ip, .sp = vm->sp};
  vm->fp->try_ctx = ctx;
  return true;
}

inline static TryCtx tear_try(VM* vm) {
  TryCtx ctx = vm->fp->try_ctx;
  vm->fp->try_ctx = new_tryctx();
  return ctx;
}

inline static void handle_error(VM* vm, Value err, char* fmt, va_list* ap) {
  vm->sp = vm->fp->try_ctx.sp;
  vm->fp->ip = vm->fp->try_ctx.handler_ip;
  vm->current_module = vm->fp->closure->func->module;
  vm->has_error = false;
  tear_try(vm);
  char buff[KB_SIZE];
  int len = vsnprintf(buff, KB_SIZE, fmt, *ap);
  va_end(*ap);
  if (err == NOTHING_VAL) {
    Value error = create_stringv(vm, &vm->strings, buff, len, false);
    push_stack(vm, error);
  } else {
    push_stack(vm, err);
  }
}

bool vm_push_frame(VM* vm, CallFrame frame) {
  return push_frame(vm, frame);
}

CallFrame vm_pop_frame(VM* vm) {
  return pop_frame(vm);
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
      .compiler = NULL,
      .builtins = NULL,
      .current_module = NULL};
  vm.sp = vm.stack;
  map_init(&vm.strings);
  map_init(&vm.modules);
  gc_init(&vm.gc);
  return vm;
}

void free_vm(VM* vm) {
  if (vm->modules.entries) {
    FREE_BUFFER(vm, vm->modules.entries, MapEntry, vm->modules.capacity);
  }
  if (vm->strings.entries) {
    FREE_BUFFER(vm, vm->strings.entries, MapEntry, vm->strings.capacity);
  }
  Obj* next;
  for (Obj* obj = vm->objects; obj != NULL; obj = next) {
    next = obj->next;
    free_object(vm, obj);
  }
  gc_free(&vm->gc);
}

bool boot_vm(VM* vm, ObjFn* func) {
  vm->is_compiling = true;
  vm->fp = vm->frames;
  vm->sp = vm->stack;
  // assumes that 'strings' hashmap has been initialized already by the compiler
  map_init(&vm->modules);
  ObjClosure* closure = create_closure(vm, func);
  ASSERT(closure->func->module, "top-level script must have a module");
  CallFrame frame = {
      .ip = func->code.bytes,
      .closure = closure,
      .stack = vm->sp,
      .try_ctx = new_tryctx()};
  push_frame(vm, frame);
  push_stack(vm, OBJ_VAL(closure));
  init_builtins(vm, closure->func->module);
  vm->is_compiling = false;
  if (vm->has_error) {
    free_vm(vm);
    fprintf(stderr, "vm boot failed.\n");
    exit(EXIT_FAILURE);
  }
  return true;
}

IResult runtime_error(VM* vm, Value err, char* fmt, ...) {
  vm->has_error = true;
  va_list ap;
  // try to recover:
  int frame_count = vm->frame_count;
  CallFrame* fp;
  while (frame_count) {
    fp = &vm->frames[frame_count - 1];
    // check for error handlers
    if (fp->try_ctx.handler_ip) {
      vm->frame_count = frame_count;
      vm->fp = fp;
      va_start(ap, fmt);
      handle_error(vm, err, fmt, &ap);
      return 0;
    }
    frame_count--;
  }
  int repeating_frames = 0;
  CallFrame* prev_frame = NULL;
  ObjString* last_file = NULL;
  size_t offset;
  fputs("Runtime Error: ", stderr);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  while (vm->frame_count) {
    if (prev_frame
        && prev_frame->closure->func->name == vm->fp->closure->func->name) {
      repeating_frames++;
      if (repeating_frames >= 4) {
        prev_frame = vm->fp;
        pop_frame(vm);
        continue;
      }
    }
    offset = vm->fp->ip - vm->fp->closure->func->code.bytes - 1;
    if (vm->frame_count > 1) {
      if (last_file != vm->fp->closure->func->module->name) {
        fprintf(
            stderr,
            "File %s\n\t",
            vm->fp->closure->func->module->name->str);
      } else {
        fputc('\t', stderr);
      }
      fprintf(
          stderr,
          "in %s(), line %d\n",
          get_func_name(vm->fp->closure->func),
          vm->fp->closure->func->code.lines[offset]);
    } else {
      // last frame
      fprintf(
          stderr,
          "File %s, line %d\n",
          vm->fp->closure->func->module->name->str,
          vm->fp->closure->func->code.lines[offset]);
    }
    last_file = vm->fp->closure->func->module->name;
    prev_frame = vm->fp;
    pop_frame(vm);
  }
  return RESULT_RUNTIME_ERROR;
}

void serde_error_cb(VM* vm, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  free_vm(vm);
  exit(EXIT_FAILURE);
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
        NOTHING_VAL,
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
    runtime_error(vm, NOTHING_VAL, "%s index not in range", type);
    return false;
  } else if ((index != (int64_t)index)) {
    runtime_error(vm, NOTHING_VAL, "%s subscript must be an integer", type);
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
      vm->sp -= 2;
      push_stack(vm, list->elems.buffer[index]);
      return true;
    }
  } else if (IS_HMAP(val)) {
    Value res = hashmap_get(AS_HMAP(val), subscript);
    if (res != NOTHING_VAL) {
      vm->sp -= 2;
      push_stack(vm, res);
      return true;
    } else {
      runtime_error(
          vm,
          NOTHING_VAL,
          "hashmap has no such key: '%s'",
          AS_STRING(value_to_string(vm, subscript))->str);
    }
  } else if (IS_STRING(val)) {
    ObjString* str = AS_STRING(val);
    int64_t index;
    if (validate_subscript(vm, subscript, str->length, "string", &index)) {
      // gc reasons
      Value new_str =
          create_stringv(vm, &vm->strings, &str->str[index], 1, false);
      vm->sp -= 2;
      push_stack(vm, new_str);
      return true;
    }
  } else {
    runtime_error(
        vm,
        NOTHING_VAL,
        "'%s' type is not subscriptable",
        get_value_type(val));
  }
  return false;
}

static bool struct_prop_access(VM* vm, Value value, Value property) {
  ObjString* prop = AS_STRING(property);
  if (IS_STRUCT(value) || IS_MODULE(value)) {
    ObjStruct* strukt = AS_STRUCT(value);
    Value res;
    if ((res = map_get(&strukt->fields, AS_STRING(property))) != NOTHING_VAL) {
      push_stack(vm, res);
      return true;
    } else {
      if (IS_STRUCT(value)) {
        runtime_error(
            vm,
            NOTHING_VAL,
            "Illegal/unknown property access '%s'",
            prop->str);
      } else {
        runtime_error(
            vm,
            NOTHING_VAL,
            "Module '%s' has no property '%s'",
            strukt->name->str,
            prop->str);
      }
    }
  } else {
    runtime_error(
        vm,
        NOTHING_VAL,
        "Invalid field accessor for type '%s'",
        get_value_type(value),
        prop->str);
  }
  return false;
}

static bool instance_prop_access(VM* vm, Value value, ObjString* property) {
  if (IS_INSTANCE(value)) {
    ObjInstance* instance = AS_INSTANCE(value);
    Value res;
    if ((res = map_get(&instance->fields, property)) != NOTHING_VAL) {
      push_stack(vm, res);
      return true;
    } else if ((map_has_key(&instance->strukt->fields, property, &res))) {
      /*
       * here, the property doesn't exist as a key in the instance's `fields` which could mean two things:
       * 1. the key wasn't assigned when creating the instance e.g. Bar {}; instead of Bar { a = something };
       * 2. the key is invalid. e.g. belongs to the struct, or isn't a field.
       * For (1.) we look up the key in the instance's struct, if the struct has such a key, and the key doesn't
       * belong to the struct (i.e. key is NOTHING_VAL), we can safely set it as `None` on the instance
       */
      if (res == NOTHING_VAL) {
        push_stack(vm, value);  // gc reasons
        push_stack(vm, OBJ_VAL(property));  // gc reasons
        map_put(&instance->fields, vm, property, NONE_VAL);
        vm->sp -= 2;  // gc reasons
        push_stack(vm, NONE_VAL);
        return true;
      }
    }
    runtime_error(
        vm,
        NOTHING_VAL,
        "Illegal/unknown property access '%s'",
        property->str);
  } else {
    runtime_error(
        vm,
        NOTHING_VAL,
        "Invalid property accessor for type '%s'",
        get_value_type(value),
        property->str);
  }
  return false;
}

static bool instance_prop_assign(VM* vm, ObjString* property, Value var) {
  if (IS_INSTANCE(var)) {
    ObjInstance* instance = AS_INSTANCE(var);
    // true if 'property' is a new key
    if (map_put(&instance->fields, vm, property, PEEK_STACK_AT(vm, 1))) {
      // check if the key exists in the instance's struct and its value is a NOTHING_VAL
      // this means instance was created with some of its fields unassigned.
      Value val;
      if (!(map_has_key(&instance->strukt->fields, property, &val)
            && val == NOTHING_VAL)) {
        map_remove(&instance->fields, property);
        runtime_error(
            vm,
            NOTHING_VAL,
            "Instance of '%s' has no property '%s'",
            instance->strukt->name->str,
            property->str);
        return false;
      }
    }
    pop_stack(vm);
    return true;
  } else {
    runtime_error(
        vm,
        NOTHING_VAL,
        "Cannot set property on type '%s'",
        get_value_type(var));
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
        NOTHING_VAL,
        "'%s' type is not subscript-assignable",
        get_value_type(var));
  }
  return false;
}

inline bool call_value(VM* vm, Value val, int argc, bool is_tco) {
  if (IS_CLOSURE(val)) {
    ObjFn* fn = AS_CLOSURE(val)->func;
    if (argc == fn->arity) {
      if (!is_tco) {
        CallFrame frame = {
            .closure = AS_CLOSURE(val),
            .stack = vm->sp - 1 - argc,
            .ip = fn->code.bytes,
            .try_ctx = new_tryctx()};
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
        NOTHING_VAL,
        "'%s' function takes %d argument(s) but got %d",
        get_func_name(fn),
        fn->arity,
        argc);
  } else if (IS_CFUNC(val)) {
    ObjCFn* fn = AS_CFUNC(val);
    if (fn->arity == argc || fn->arity < 0) {
      Value res = fn->fn(vm, argc, vm->sp - argc);
      // only push the result if no error occurred.
      // we use NOTHING_VAL return value as an error flag
      if (res != NOTHING_VAL) {
        vm->sp -= (argc + 1);
        push_stack(vm, res);
      }
      return !vm->has_error;
    }
    runtime_error(
        vm,
        NOTHING_VAL,
        "'%s' function takes %d argument(s) but got %d",
        fn->name,
        fn->arity,
        argc);
  } else {
    runtime_error(
        vm,
        NOTHING_VAL,
        "'%s' type is not callable",
        get_value_type(val));
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
#if defined(EVE_DEBUG_EXECUTION)
  print_stack(vm);
  dis_instruction(
      &vm->fp->closure->func->code,
      (int)(vm->fp->ip - vm->fp->closure->func->code.bytes));
#endif
  VM_LOOP {
    case $LOAD_CONST: {
      push_stack(vm, READ_CONST(vm));
      DISPATCH();
    }
    case $DEFINE_GLOBAL: {
      ObjString* var = READ_STRING(vm);
      map_put(&vm->current_module->fields, vm, var, PEEK_STACK(vm));
      pop_stack(vm);
      DISPATCH();
    }
    case $GET_GLOBAL: {
      ObjString* var = READ_STRING(vm);
      Value val;
      if ((val = map_get(&vm->current_module->fields, var)) != NOTHING_VAL) {
        push_stack(vm, val);
      } else {
        runtime_error(vm, NOTHING_VAL, "Name '%s' is not defined", var->str);
        TRY_RECOVER(vm)
      }
      DISPATCH();
    }
    case $GET_LOCAL: {
      push_stack(vm, vm->fp->stack[READ_BYTE(vm)]);
      DISPATCH();
    }
    case $GET_UPVALUE: {
      push_stack(vm, *vm->fp->closure->env[READ_BYTE(vm)]->location);
      DISPATCH();
    }
    case $SET_GLOBAL: {
      ObjString* var = READ_STRING(vm);
      if (map_put(&vm->current_module->fields, vm, var, PEEK_STACK(vm))) {
        // true if key is new - new insertion, false if key already exists
        map_remove(&vm->current_module->fields, var);
        return runtime_error(
            vm,
            NOTHING_VAL,
            "use of undefined variable '%s'",
            var->str);
        TRY_RECOVER(vm)
      }
      DISPATCH();
    }
    case $SET_LOCAL: {
      vm->fp->stack[READ_BYTE(vm)] = PEEK_STACK(vm);
      DISPATCH();
    }
    case $SET_UPVALUE: {
      *vm->fp->closure->env[READ_BYTE(vm)]->location = PEEK_STACK(vm);
      DISPATCH();
    }
    case $SET_SUBSCRIPT: {
      Value subscript = PEEK_STACK(vm);
      Value var = PEEK_STACK_AT(vm, 1);  // gc reasons
      Value value = PEEK_STACK_AT(vm, 2);  // gc reasons
      if (!perform_subscript_assign(vm, var, subscript, value)) {
        TRY_RECOVER(vm)
      }
      pop_stack(vm);  // gc reasons
      pop_stack(vm);  // gc reasons
      DISPATCH();
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
      DISPATCH();
    }
    case $TAIL_CALL:
    case $CALL: {
      int argc = READ_BYTE(vm);
      if (!call_value(vm, PEEK_STACK_AT(vm, argc), argc, inst == $TAIL_CALL)) {
        TRY_RECOVER(vm)
      }
      DISPATCH();
    }
    case $ADD: {
      BINARY_OP(vm, +, NUMBER_VAL)
      DISPATCH();
    }
    case $SUBTRACT: {
      BINARY_OP(vm, -, NUMBER_VAL)
      DISPATCH();
    }
    case $DIVIDE: {
      BINARY_OP(vm, /, NUMBER_VAL)
      DISPATCH();
    }
    case $MULTIPLY: {
      BINARY_OP(vm, *, NUMBER_VAL)
      DISPATCH();
    }
    case $LESS: {
      BINARY_OP(vm, <, BOOL_VAL);
      DISPATCH();
    }
    case $GREATER: {
      BINARY_OP(vm, >, BOOL_VAL);
      DISPATCH();
    }
    case $LESS_OR_EQ: {
      BINARY_OP(vm, <=, BOOL_VAL);
      DISPATCH();
    }
    case $GREATER_OR_EQ: {
      BINARY_OP(vm, >=, BOOL_VAL);
      DISPATCH();
    }
    case $POW: {
      Value b = pop_stack(vm);
      Value a = pop_stack(vm);
      BINARY_CHECK(vm, **, a, b, IS_NUMBER);
      double res = pow(AS_NUMBER(a), AS_NUMBER(b));
      push_stack(vm, NUMBER_VAL(res));
      DISPATCH();
    }
    case $MOD: {
      Value b = pop_stack(vm);
      Value a = pop_stack(vm);
      BINARY_CHECK(vm, %, a, b, IS_NUMBER);
      double res = fmod(AS_NUMBER(a), AS_NUMBER(b));
      push_stack(vm, NUMBER_VAL(res));
      DISPATCH();
    }
    case $NOT: {
      Value v = pop_stack(vm);
      push_stack(vm, BOOL_VAL(value_falsy(v)));
      DISPATCH();
    }
    case $NEGATE: {
      Value v = pop_stack(vm);
      UNARY_CHECK(vm, -, v, IS_NUMBER);
      push_stack(vm, NUMBER_VAL(-AS_NUMBER(v)));
      DISPATCH();
    }
    case $BW_INVERT: {
      Value v = pop_stack(vm);
      UNARY_CHECK(vm, ~, v, IS_NUMBER);
      push_stack(vm, NUMBER_VAL((~(int64_t)AS_NUMBER(v))));
      DISPATCH();
    }
    case $EQ: {
      Value b = pop_stack(vm);
      Value a = pop_stack(vm);
      push_stack(vm, BOOL_VAL(value_equal(a, b)));
      DISPATCH();
    }
    case $NOT_EQ: {
      Value b = pop_stack(vm);
      Value a = pop_stack(vm);
      push_stack(vm, BOOL_VAL(!value_equal(a, b)));
      DISPATCH();
    }
    case $POP: {
      pop_stack(vm);
      DISPATCH();
    }
    case $POP_N: {
      vm->sp -= READ_BYTE(vm);
      DISPATCH();
    }
    case $SUBSCRIPT: {
      Value subscript = PEEK_STACK(vm);
      Value val = PEEK_STACK_AT(vm, 1);
      if (!perform_subscript(vm, val, subscript)) {
        TRY_RECOVER(vm)
      }
      DISPATCH();
    }
    case $GET_FIELD: {
      Value property = READ_CONST(vm);
      Value value = pop_stack(vm);
      if (!struct_prop_access(vm, value, property)) {
        TRY_RECOVER(vm)
      }
      DISPATCH();
    }
    case $GET_PROPERTY: {
      Value property = READ_CONST(vm);
      Value value = pop_stack(vm);
      if (!instance_prop_access(vm, value, AS_STRING(property))) {
        TRY_RECOVER(vm)
      }
      DISPATCH();
    }
    case $SET_PROPERTY: {
      Value property = READ_CONST(vm);
      Value var = PEEK_STACK(vm);
      if (!instance_prop_assign(vm, AS_STRING(property), var)) {
        TRY_RECOVER(vm)
      }
      DISPATCH();
    }
    case $DISPLAY: {
      byte_t len = READ_BYTE(vm);
      for (int i = 0; i < len; i++) {
        print_value(PEEK_STACK_AT(vm, i));
        if (i < len - 1) {
          printf(" ");
        }
      }
      printf("\n");
      vm->sp -= len;
      DISPATCH();
    }
    case $ASSERT: {
      Value test = PEEK_STACK(vm);  // gc reasons
      Value msg = PEEK_STACK_AT(vm, 1);  // gc reasons
      if (value_falsy(test)) {
        runtime_error(
            vm,
            NOTHING_VAL,
            "Assertion Failed: %s",
            AS_STRING(value_to_string(vm, msg))->str);
        TRY_RECOVER(vm)
      }
      pop_stack(vm);  // gc reasons
      pop_stack(vm);  // gc reasons
      DISPATCH();
    }
    case $JMP: {
      uint16_t offset = READ_SHORT(vm);
      vm->fp->ip += offset;
      DISPATCH();
    }
    case $JMP_FALSE: {
      uint16_t offset = READ_SHORT(vm);
      if (value_falsy(PEEK_STACK(vm))) {
        vm->fp->ip += offset;
      }
      DISPATCH();
    }
    case $JMP_FALSE_OR_POP: {
      uint16_t offset = READ_SHORT(vm);
      if (value_falsy(PEEK_STACK(vm))) {
        vm->fp->ip += offset;
      } else {
        pop_stack(vm);
      }
      DISPATCH();
    }
    case $LOOP: {
      uint16_t offset = READ_SHORT(vm);
      vm->fp->ip -= offset;
      DISPATCH();
    }
    case $SET_TRY: {
      set_try(vm, READ_SHORT(vm));
      DISPATCH();
    }
    case $TEAR_TRY: {
      tear_try(vm);
      DISPATCH();
    }
    case $THROW: {
      Value val = pop_stack(vm);
      if (IS_STRING(val)) {
        runtime_error(vm, val, "%s", AS_STRING(val)->str);
      } else {
        runtime_error(vm, val, "");
      }
      TRY_RECOVER(vm)
    }
    case $BUILD_LIST: {
      ObjList* list = create_list(vm, READ_BYTE(vm));
      for (int i = 0; i < list->elems.length; i++) {
        list->elems.buffer[i] = PEEK_STACK_AT(vm, i);
      }
      vm->sp -= list->elems.length;
      push_stack(vm, OBJ_VAL(list));
      DISPATCH();
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
      DISPATCH();
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
      DISPATCH();
    }
    case $BUILD_STRUCT: {
      ObjString* name = READ_STRING(vm);
      byte_t field_count = READ_BYTE(vm) * 2;  // k-v pairs
      ObjStruct* strukt = create_struct(vm, name);
      push_stack(vm, OBJ_VAL(strukt));  // gc reasons
      ObjString* var;
      Value val;
      for (byte_t i = 0; i < field_count; i += 2) {
        val = PEEK_STACK_AT(vm, i + 1);
        var = AS_STRING(PEEK_STACK_AT(vm, i + 2));
        if (!map_put(&strukt->fields, vm, var, val)) {
          runtime_error(
              vm,
              NOTHING_VAL,
              "Duplicate field '%s'\n"
              "struct fields must be unique irrespective of meta-type",
              var->str);
          TRY_RECOVER(vm)
        }
      }
      vm->sp -= field_count + 1;  // +1 for strukt (gc reasons)
      push_stack(vm, OBJ_VAL(strukt));
      DISPATCH();
    }
    case $BUILD_INSTANCE: {
      Value var = PEEK_STACK(vm);  // gc reasons
      byte_t field_count = READ_BYTE(vm) * 2;  // k-v pairs
      if (!IS_STRUCT(var)) {
        runtime_error(
            vm,
            NOTHING_VAL,
            "Cannot instantiate '%s' type",
            get_value_type(var));
        TRY_RECOVER(vm)
      }
      ObjString* key;
      Value val, check;
      ObjStruct* strukt = AS_STRUCT(var);
      ObjInstance* instance = create_instance(vm, strukt);
      push_stack(vm, OBJ_VAL(instance));  // gc reasons
      for (int i = 0; i < field_count; i += 2) {
        val = PEEK_STACK_AT(vm, i + 2);
        key = AS_STRING(PEEK_STACK_AT(vm, i + 3));
        if (map_has_key(&strukt->fields, key, &check) && check == NOTHING_VAL) {
          // only store fields with NOTHING_VAL value flag in the instance's field
          map_put(&instance->fields, vm, key, val);
        } else {
          runtime_error(
              vm,
              NOTHING_VAL,
              "Illegal/unknown instance property access '%s'",
              key->str);
          TRY_RECOVER(vm)
        }
      }
      vm->sp -= field_count + 2;  // +2 for var and instance (gc reasons)
      push_stack(vm, OBJ_VAL(instance));
      DISPATCH();
    }
    case $CLOSE_UPVALUE: {
      close_upvalues(vm, vm->sp - 1);
      pop_stack(vm);
      DISPATCH();
    }
    case $BW_XOR: {
      Value b = pop_stack(vm);
      Value a = pop_stack(vm);
      BINARY_CHECK(vm, ^, a, b, IS_NUMBER);
      push_stack(
          vm,
          NUMBER_VAL(((int64_t)AS_NUMBER(a) ^ (int64_t)AS_NUMBER(b))));
      DISPATCH();
    }
    case $BW_OR: {
      Value b = pop_stack(vm);
      Value a = pop_stack(vm);
      BINARY_CHECK(vm, |, a, b, IS_NUMBER);
      push_stack(
          vm,
          NUMBER_VAL(((int64_t)AS_NUMBER(a) | (int64_t)AS_NUMBER(b))));
      DISPATCH();
    }
    case $BW_AND: {
      Value b = pop_stack(vm);
      Value a = pop_stack(vm);
      BINARY_CHECK(vm, &, a, b, IS_NUMBER);
      push_stack(
          vm,
          NUMBER_VAL(((int64_t)AS_NUMBER(a) & (int64_t)AS_NUMBER(b))));
      DISPATCH();
    }
    case $BW_LSHIFT: {
      Value b = pop_stack(vm);
      Value a = pop_stack(vm);
      BINARY_CHECK(vm, <<, a, b, IS_NUMBER);
      push_stack(
          vm,
          NUMBER_VAL((double)((int64_t)AS_NUMBER(a) << (int64_t)AS_NUMBER(b))));
      DISPATCH();
    }
    case $BW_RSHIFT: {
      Value b = pop_stack(vm);
      Value a = pop_stack(vm);
      BINARY_CHECK(vm, >>, a, b, IS_NUMBER);
      push_stack(
          vm,
          NUMBER_VAL((double)((int64_t)AS_NUMBER(a) >> (int64_t)AS_NUMBER(b))));
      DISPATCH();
    }
    default:
      UNREACHABLE("unknown opcode");
  }
  END_BRACE
}

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONST
#undef READ_STRING
#undef PEEK_STACK
#undef PEEK_STACK_AT
#undef BINARY_OP
#undef BINARY_CHECK
#undef UNARY_CHECK
#undef VM_LOOP
#undef DISPATCH
#undef END_BRACE
#undef KB_SIZE
#undef TRY_RECOVER
