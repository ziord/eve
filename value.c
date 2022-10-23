#include "value.h"

void init_value_pool(ValuePool* vp) {
  vp->values = NULL;
  vp->length = 0;
  vp->capacity = 0;
}

void free_value_pool(ValuePool* vp, VM* vm) {
  FREE(vm, vp->values, Value);
  init_value_pool(vp);
}

int write_value(ValuePool* vp, Value v, VM* vm) {
  if (vp->length >= vp->capacity) {
    vp->capacity = GROW_CAPACITY(vp->capacity);
    vp->values =
        GROW_BUFFER(vm, vp->values, Value, vp->length, vp->capacity);
  }
  vp->values[vp->length] = v;
  return vp->length++;
}

char* get_value_type(Value val) {
  if (IS_NUMBER(val)) {
    return "number";
  } else if (IS_BOOL(val)) {
    return "bool";
  }
  UNREACHABLE("unknown value type");
}

void print_value(Value val) {
  if (IS_BOOL(val)) {
    printf("%s", AS_BOOL(val) ? "true" : "false");
  } else if (IS_NUMBER(val)) {
    printf("%g", AS_NUMBER(val));
  }
}