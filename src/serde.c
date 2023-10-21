#include "serde.h"

#define VALUE_TYPE (0xf5)
#define VALUE_POOL_END (0xfff)
#define UNUSED(_v_) (_v_)
#define SERDE_ASSERT(sd, tst, ...) \
  (!(tst) ? (sd)->callback( \
       (sd)->vm, \
       (sd)->mode == SD_SERIALIZE ? "Serialization Error: %s" \
                                  : "Deserialization Error: %s", \
       __VA_ARGS__) \
          : (void)0)

#define MAGIC_BITS \
  (0x811c9dc5 \
   | (EVE_VERSION_MAJOR | EVE_VERSION_MINOR | EVE_VERSION_MINOR) \
   | (NONE_VAL | NOTHING_VAL | FALSE_VAL | TRUE_VAL))

void ser_vpool(EveSerde* serde, ValuePool* vp);
void ser_value(EveSerde* serde, Value value);
void ser_object(EveSerde* serde, Obj* obj);
void de_vpool(EveSerde* serde, ValuePool* vp);
Value de_value(EveSerde* serde);
Value de_object(EveSerde* serde);

FILE* open_file(const char* fname, char* mode) {
  FILE* file = fopen(fname, mode);
  if (!file) {
    fprintf(stderr, "Could not open file '%s'", fname);
  }
  return file;
}

int peek_file(FILE* file) {
  int c = fgetc(file);
  ungetc(c, file);
  return c;
}

void init_serde(EveSerde* serde, SerdeMode mode, VM* vm, error_cb cb) {
  serde->file = NULL;
  serde->mode = mode;
  serde->callback = cb;
  serde->vm = vm;
}

void free_serde(EveSerde* serde) {
  if (serde->file) {
    fclose(serde->file);
  }
  init_serde(serde, SD_INIT, NULL, NULL);
}

void write_magic_bits(EveSerde* serde) {
  uint64_t bits = MAGIC_BITS;
  int version[] = {EVE_VERSION_MAJOR, EVE_VERSION_MINOR, EVE_VERSION_PATCH};
  fwrite(&bits, sizeof(uint64_t), 1, serde->file);
  fwrite(&version, sizeof(int), 3, serde->file);
}

static inline bool check_magic_bits(EveSerde* serde) {
  uint64_t bits;
  fread(&bits, sizeof(uint64_t), 1, serde->file);
  return bits == MAGIC_BITS;
}

static inline bool check_version(EveSerde* serde) {
  int version[3];
  fread(&version, sizeof(int), 3, serde->file);
  return (
      version[0] == EVE_VERSION_MAJOR && version[1] == EVE_VERSION_MINOR
      && version[2] == EVE_VERSION_PATCH);
}

void ser_code(EveSerde* serde, Code* code) {
  /*
   * .length length
   * .capacity capacity
   * .code bytecode
   * .code bytecode
   * ...
   * .line line
   * .line line
   * ...
   * .serialise value-pool
   */
  int buff[] = {code->length, code->capacity};
  fwrite(buff, sizeof(int), 2, serde->file);
  fwrite(code->bytes, sizeof(byte_t), code->length, serde->file);
  fwrite(code->lines, sizeof(int), code->length, serde->file);
  ser_vpool(serde, &code->vpool);
}

void de_code(EveSerde* serde, Code* code) {
  int buff[2];
  fread(buff, sizeof(int), 2, serde->file);
  code->length = buff[0];
  code->capacity = buff[1];
  code->bytes = GROW_BUFFER(serde->vm, NULL, byte_t, 0, code->capacity);
  code->lines = GROW_BUFFER(serde->vm, NULL, int, 0, code->capacity);
  fread(code->bytes, sizeof(byte_t), code->length, serde->file);
  fread(code->lines, sizeof(int), code->length, serde->file);
  de_vpool(serde, &code->vpool);
}

void ser_vpool(EveSerde* serde, ValuePool* vp) {
  /*
   * .length length
   * .capacity capacity
   * .serialise value
   * .serialise value
   * ...
   */
  int buff[] = {vp->length, vp->capacity};
  fwrite(buff, sizeof(int), 2, serde->file);
  for (int i = 0; i < vp->length; i++) {
    ser_value(serde, vp->values[i]);
  }
  int x = VALUE_POOL_END;
  fwrite(&x, sizeof(int), 1, serde->file);
}

void de_vpool(EveSerde* serde, ValuePool* vp) {
  int buff[2];
  fread(buff, sizeof(int), 2, serde->file);
  vp->length = buff[0];
  vp->capacity = buff[1];
  vp->values = GROW_BUFFER(serde->vm, NULL, Value, 0, vp->capacity);
  for (int i = 0; i < vp->length; i++) {
    vp->values[i] = de_value(serde);
  }
  fread(buff, sizeof(int), 1, serde->file);
  SERDE_ASSERT(
      serde,
      buff[0] == VALUE_POOL_END,
      "Deserialize expected <<VALUE_POOL_END>>");
}

void ser_value(EveSerde* serde, Value value) {
  if (IS_OBJ(value)) {
    ser_object(serde, AS_OBJ(value));
  } else {
    fputc(VALUE_TYPE, serde->file);
    fwrite(&value, sizeof(Value), 1, serde->file);
  }
}

Value de_value(EveSerde* serde) {
  int type = peek_file(serde->file);
  if (type == VALUE_TYPE) {
    Value value;
    fgetc(serde->file);  // read off VALUE_TYPE
    fread(&value, sizeof(Value), 1, serde->file);
    return value;
  } else {
    // is object
    return de_object(serde);
  }
}

void ser_obj(EveSerde* serde, Obj* obj) {
  fputc(obj->type, serde->file);
}

ObjTy de_obj(EveSerde* serde) {
  return fgetc(serde->file);
}

void ser_string(EveSerde* serde, ObjString* string) {
  /*
   * .type type
   * .length length
   * .hash hash
   * .chars str-chars
   */
  ser_obj(serde, &string->obj);
  fwrite(&string->length, sizeof(int), 1, serde->file);
  fwrite(&string->hash, sizeof(uint32_t), 1, serde->file);
  fwrite(string->str, sizeof(char), string->length, serde->file);
}

ObjString* de_string(EveSerde* serde) {
  SERDE_ASSERT(
      serde,
      de_obj(serde) == OBJ_STR,
      "Deserialize expected string type");
  int len;
  uint32_t hash;
  fread(&len, sizeof(int), 1, serde->file);
  fread(&hash, sizeof(uint32_t), 1, serde->file);
  char* str = alloc(NULL, len + 1);
  fread(str, sizeof(char), len, serde->file);
  ObjString* string =
      create_de_string(serde->vm, &serde->vm->strings, str, len, hash);
  return string;
}

void ser_struct(EveSerde* serde, ObjStruct* strukt) {
  /*
   * .type type
   * .serialise name (ObjString)
   * .serialise fields (ObjHashMap)
   */
  ser_obj(serde, &strukt->obj);
  ser_string(serde, strukt->name);
}

Value de_struct(EveSerde* serde, ObjTy ty) {
  SERDE_ASSERT(
      serde,
      de_obj(serde) == ty,
      (ty == OBJ_STRUCT ? "Expected 'struct' type"
                        : "Expected 'module' type"));
  ObjString* name = de_string(serde);
  ObjStruct* strukt = create_struct(serde->vm, name);
  strukt->obj.type = ty;
  return OBJ_VAL(strukt);
}

void ser_module(EveSerde* serde, ObjStruct* strukt) {
  ser_struct(serde, strukt);
}

Value de_module(EveSerde* serde) {
  return de_struct(serde, OBJ_MODULE);
}

void ser_fn(EveSerde* serde, ObjFn* fn) {
  /*
   * .type type
   * .arity arity
   * .env-len len
   * .serialise name (ObjString)
   * .serialise code
   * .serialise module (ObjStruct)
   */
  static int i = 0;
  ser_obj(serde, &fn->obj);
  fputc(fn->arity, serde->file);
  fputc(fn->env_len, serde->file);
  if (fn->name) {
    fputc(1, serde->file);
    ser_string(serde, fn->name);
  } else {
    fputc(0, serde->file);
  }
  ser_code(serde, &fn->code);
  if (i == 0) {
    // $$SERDE_MODULE_HACK$$
    // we only need to ser this once, afterwards,
    // the others would be initialized through this.
    // this is an effective hack to ensure that all defined functions
    // in the current module are initialized to the same module
    ser_module(serde, fn->module);
  }
  i++;
}

ObjFn* de_fn(EveSerde* serde) {
  ObjTy type = de_obj(serde);
  SERDE_ASSERT(serde, type == OBJ_FN, "start type should be function type");
  ObjFn* fn = create_function(serde->vm);
  fn->arity = fgetc(serde->file);
  fn->env_len = fgetc(serde->file);
  bool has_name = fgetc(serde->file);
  if (has_name) {
    fn->name = de_string(serde);
  }
  de_code(serde, &fn->code);
  // hack to ensure that all defined functions in the current module
  // are initialized to the same module
  if (!serde->vm->current_module) {
    fn->module = serde->vm->current_module = AS_MODULE(de_module(serde));
  } else {
    fn->module = serde->vm->current_module;
  }
  return fn;
}

void ser_object(EveSerde* serde, Obj* obj) {
  switch (obj->type) {
    case OBJ_STR:
      ser_string(serde, (ObjString*)obj);
      break;
    case OBJ_STRUCT:
      ser_struct(serde, (ObjStruct*)obj);
      break;
    case OBJ_MODULE:
      ser_module(serde, (ObjStruct*)obj);
      break;
    case OBJ_FN:
      ser_fn(serde, (ObjFn*)obj);
      break;
    case OBJ_LIST:
    case OBJ_CLOSURE:
    case OBJ_UPVALUE:
    case OBJ_INSTANCE:
    case OBJ_HMAP:
    case OBJ_CFN:
      SERDE_ASSERT(serde, false, "Unreachable: object type");
  }
}

Value de_object(EveSerde* serde) {
  ObjTy ty = peek_file(serde->file);
  switch (ty) {
    case OBJ_STR:
      return OBJ_VAL(de_string(serde));
    case OBJ_STRUCT:
      return de_struct(serde, ty);
    case OBJ_MODULE:
      return de_module(serde);
    case OBJ_FN:
      return OBJ_VAL(de_fn(serde));
    case OBJ_LIST:
    case OBJ_CLOSURE:
    case OBJ_UPVALUE:
    case OBJ_INSTANCE:
    case OBJ_CFN:
    case OBJ_HMAP:
    default:
      SERDE_ASSERT(serde, false, "Unreachable: object type");
  }
  return UNUSED(NOTHING_VAL);
}

bool serialize(EveSerde* serde, const char* filename, ObjFn* script) {
  ASSERT(serde->vm, "Expected serde initialized with VM");
  ASSERT(serde->callback, "Expected serde initialized with error callback");
  ASSERT(
      serde->mode == SD_SERIALIZE,
      "Expected serde initialized in 'serialize' mode");
  FILE* file = open_file(filename, "wb");
  if (!file) {
    return false;
  }
  serde->file = file;
  write_magic_bits(serde);
  ser_object(serde, (Obj*)script);
  fflush(serde->file);
  fclose(file);
  return true;
}

ObjFn* deserialize(EveSerde* serde, const char* filename) {
  ASSERT(serde->vm, "Expected serde initialized with VM");
  ASSERT(serde->callback, "Expected serde initialized with error callback");
  ASSERT(
      serde->mode == SD_DESERIALIZE,
      "Expected serde initialized in 'deserialize' mode");
  FILE* file = open_file(filename, "rb");
  if (!file) {
    return false;
  }
  serde->file = file;
  if (!check_magic_bits(serde) || !check_version(serde)) {
    return UNUSED(NULL);
  }
  ObjFn* func = de_fn(serde);
  return func;
}
