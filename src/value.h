#ifndef EVE_VALUE_H
#define EVE_VALUE_H

#include <inttypes.h>
#include <string.h>

#include "defs.h"
#include "memory.h"
#include "opcode.h"
#include "util.h"

typedef uint64_t Value;

#define QNAN ((uint64_t)0x7ffc000000000000)
#define SIGN_BIT (0x8000000000000000)

// TAGS
#define TAG_NONE (0x1)  // 01
#define TAG_FALSE (0x2)  // 10
#define TAG_TRUE (0x3)  // 11
#define TAG_NOTHING (0x4)  // 100
#define TAG_OBJ (SIGN_BIT | QNAN)

#define NONE_VAL ((Value)(uint64_t)(QNAN | TAG_NONE))
#define TRUE_VAL ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define FALSE_VAL ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define NOTHING_VAL ((Value)(uint64_t)(QNAN | TAG_NOTHING))

#define NUMBER_VAL(num) (num_to_val(num))
#define AS_NUMBER(val) (val_to_num(val))
#define IS_NUMBER(val) (((val)&QNAN) != QNAN)

#define BOOL_VAL(v) ((v) ? TRUE_VAL : FALSE_VAL)
#define AS_BOOL(b) ((b) == TRUE_VAL)
#define IS_BOOL(b) (((b) | 1) == TRUE_VAL)

#define IS_NONE(v) ((v) == NONE_VAL)
#define IS_NOTHING(v) ((v) == NOTHING_VAL)

#define OBJ_VAL(obj) ((Value)((uint64_t)(uintptr_t)(obj) | TAG_OBJ))
#define AS_OBJ(val) ((Obj*)(uintptr_t)((val) & ~(TAG_OBJ)))
#define IS_OBJ(val) (((val) & (TAG_OBJ)) == (TAG_OBJ))

#define IS_STRING(val) (is_object_type(val, OBJ_STR))
#define IS_LIST(val) (is_object_type(val, OBJ_LIST))
#define IS_HMAP(val) (is_object_type(val, OBJ_HMAP))
#define IS_FUNC(val) (is_object_type(val, OBJ_FN))
#define IS_CLOSURE(val) (is_object_type(val, OBJ_CLOSURE))
#define IS_STRUCT(val) (is_object_type(val, OBJ_STRUCT))
#define IS_INSTANCE(val) (is_object_type(val, OBJ_INSTANCE))
#define IS_CFUNC(val) (is_object_type(val, OBJ_CFN))
#define IS_MODULE(val) (is_object_type(val, OBJ_MODULE))

#define AS_STRING(val) ((ObjString*)(AS_OBJ(val)))
#define AS_LIST(val) ((ObjList*)(AS_OBJ(val)))
#define AS_HMAP(val) ((ObjHashMap*)(AS_OBJ(val)))
#define AS_FUNC(val) ((ObjFn*)(AS_OBJ(val)))
#define AS_CLOSURE(val) ((ObjClosure*)(AS_OBJ(val)))
#define AS_STRUCT(val) ((ObjStruct*)(AS_OBJ(val)))
#define AS_INSTANCE(val) ((ObjInstance*)(AS_OBJ(val)))
#define AS_CFUNC(val) ((ObjCFn*)(AS_OBJ(val)))
#define AS_MODULE(val) AS_STRUCT(val)

#define CREATE_OBJ(vm, obj_struct, obj_ty, size) \
  (obj_struct*)create_object(vm, obj_ty, size)

#define value_equal(a, b) \
  ((IS_NUMBER((a)) && IS_NUMBER((b))) ? (AS_NUMBER((a)) == AS_NUMBER((b))) \
                                      : (a) == (b))

#define create_stringv(vm, map, str, len, is_alloc) \
  OBJ_VAL(create_string(vm, map, str, len, is_alloc))

#define MAP_LOAD_FACTOR (0.75)

typedef struct {
  int length;
  int capacity;
  Value* values;
} ValuePool;

typedef struct Code {
  int length;
  int capacity;
  int* lines;
  byte_t* bytes;
  ValuePool vpool;
} Code;

typedef struct {
  int length;
  int capacity;
  Value buffer[];
} ArrayBuffer;

typedef enum {
  OBJ_STR,
  OBJ_LIST,
  OBJ_HMAP,
  OBJ_FN,
  OBJ_CLOSURE,
  OBJ_UPVALUE,
  OBJ_STRUCT,
  OBJ_INSTANCE,
  OBJ_MODULE,
  OBJ_CFN,
} ObjTy;

typedef struct Obj {
  ObjTy type;
  bool marked;
  struct Obj* next;
} Obj;

typedef struct {
  Obj obj;
  uint32_t hash;
  int length;
  char* str;
} ObjString;

typedef struct {
  Obj obj;
  ArrayBuffer elems;
} ObjList;

typedef struct {
  Value key;
  Value value;
} HashEntry;

typedef struct {
  Obj obj;
  int length;
  int capacity;
  HashEntry* entries;
} ObjHashMap;

typedef struct MapEntry {
  ObjString* key;
  Value value;
} MapEntry;

typedef struct Map {
  MapEntry* entries;
  int capacity;
  int length;
} Map;

typedef struct {
  Obj obj;
  Map fields;
  ObjString* name;
} ObjStruct;

typedef struct {
  Obj obj;
  int arity;
  int env_len;
  Code code;
  ObjString* name;
  ObjStruct* module;
} ObjFn;

typedef Value (*CFn)(VM* vm, int argc, const Value* args);

typedef struct {
  Obj obj;
  int arity;
  CFn fn;
  const char* name;
} ObjCFn;

typedef struct ObjUpvalue {
  Obj obj;
  Value* location;
  Value value;
  struct ObjUpvalue* next;
} ObjUpvalue;

typedef ObjUpvalue** Env;

typedef struct {
  Obj obj;
  int env_len;
  ObjFn* func;
  Env env;
} ObjClosure;

typedef struct {
  Obj obj;
  Map fields;
  ObjStruct* strukt;
} ObjInstance;

inline static Value num_to_val(double num) {
  return *((Value*)&(num));
}

inline static double val_to_num(Value val) {
  return *((double*)&(val));
}

inline static bool is_object_type(Value c, ObjTy type) {
  return IS_OBJ(c) && AS_OBJ(c)->type == type;
}

void init_code(Code* code);
void free_code(Code* code, VM* vm);
void write_code(Code* code, byte_t byte, int line, VM* vm);
void init_value_pool(ValuePool* vp);
void free_value_pool(ValuePool* vp, VM* vm);
int write_value(ValuePool* vp, Value v, VM* vm);
char* get_value_type(Value val);
void print_value(Value val);
void print_object(Value val, Obj* obj);
//bool value_falsy(Value v);
//bool value_equal(Value a, Value b);
Value object_to_string(VM* vm, Value val);
Value value_to_string(VM* vm, Value val);
Obj* create_object(VM* vm, ObjTy ty, size_t size);
void free_object(VM* vm, Obj* obj);
ObjString* create_string(VM* vm, Map* map, char* str, int len, bool is_alloc);
ObjString*
create_de_string(VM* vm, Map* map, char* str, int len, uint32_t hash);
ObjList* create_list(VM* vm, int len);
ObjHashMap* create_hashmap(VM* vm);
ObjFn* create_function(VM* vm);
ObjClosure* create_closure(VM* vm, ObjFn* func);
ObjUpvalue* create_upvalue(VM* vm, Value*);
ObjStruct* create_struct(VM* vm, ObjString* name);
ObjInstance* create_instance(VM* vm, ObjStruct* strukt);
ObjCFn* create_cfn(VM* vm, CFn fn, int arity, const char* name);
ObjStruct* create_module(VM* vm, ObjString* name);
char* get_func_name(ObjFn* fn);
void hashmap_init(ObjHashMap* table);
bool hashmap_put(ObjHashMap* table, VM* vm, Value key, Value value);
Value hashmap_get(ObjHashMap* table, Value key);
void hashmap_get_keys(ObjHashMap* table, ObjList* list);

#endif  // EVE_VALUE_H
