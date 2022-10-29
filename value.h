#ifndef EVE_VALUE_H
#define EVE_VALUE_H

#include <inttypes.h>
#include <string.h>

#include "defs.h"
#include "memory.h"
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
#define AS_STRING(val) ((ObjString*)(AS_OBJ(val)))

#define CREATE_OBJ(vm, obj_struct, obj_ty, size) \
  (obj_struct*)create_object(vm, obj_ty, size)

typedef struct {
  int length;
  int capacity;
  Value* values;
} ValuePool;

typedef enum { OBJ_STR } ObjTy;

typedef struct Obj {
  ObjTy type;
  struct Obj* next;
} Obj;

typedef struct {
  Obj obj;
  uint32_t hash;
  int len;
  char* str;
} ObjString;

inline static Value num_to_val(double num) {
  return *((Value*)&(num));
}

inline static double val_to_num(Value val) {
  return *((double*)&(val));
}

inline static bool is_object_type(Value c, ObjTy type) {
  return IS_OBJ(c) && AS_OBJ(c)->type == type;
}

void init_value_pool(ValuePool* vp);
void free_value_pool(ValuePool* vp, VM* vm);
int write_value(ValuePool* vp, Value v, VM* vm);
char* get_value_type(Value val);
void print_value(Value val);
bool value_falsy(Value v);
bool value_equal(Value a, Value b);
uint32_t hash_string(const char* str, int len);
uint32_t hash_value(Value v);

#endif  // EVE_VALUE_H
