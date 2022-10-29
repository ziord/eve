#include "value.h"

#include "vm.h"

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

char* get_object_type(Obj* obj) {
  switch (obj->type) {
    case OBJ_STR:
      return "string";
    case OBJ_LIST:
      return "list";
    default:
      UNREACHABLE("unknown object type");
  }
}

char* get_value_type(Value val) {
  if (IS_OBJ(val)) {
    return get_object_type(AS_OBJ(val));
  } else if (IS_NUMBER(val)) {
    return "number";
  } else if (IS_BOOL(val)) {
    return "bool";
  } else {
    UNREACHABLE("unknown value type");
  }
}

void print_object(Value val, Obj* obj) {
  switch (obj->type) {
    case OBJ_STR: {
      printf("\"%s\"", AS_STRING(val)->str);
      break;
    }
    case OBJ_LIST: {
      ObjList* list = AS_LIST(val);
      printf("[");
      for (int i = 0; i < list->elems.length; i++) {
        print_value(list->elems.buffer[i]);
        if (i < list->elems.length - 1) {
          printf(", ");
        }
      }
      printf("]");
      break;
    }
    default:
      UNREACHABLE("print: unknown object type");
  }
}

void print_value(Value val) {
  if (IS_OBJ(val)) {
    print_object(val, AS_OBJ(val));
  } else if (IS_BOOL(val)) {
    printf("%s", AS_BOOL(val) ? "true" : "false");
  } else if (IS_NUMBER(val)) {
    printf("%g", AS_NUMBER(val));
  } else if (IS_NONE(val)) {
    printf("None");
  } else {
    UNREACHABLE("print: unknown value type");
  }
}

bool value_falsy(Value v) {
  return (
      IS_NUMBER(v) && !AS_NUMBER(v) || (IS_BOOL(v) && !AS_BOOL(v))
      || IS_NONE(v));
}

bool value_equal(Value a, Value b) {
  if (IS_NUMBER(a) && IS_NUMBER(b)) {
    return AS_NUMBER(a) == AS_NUMBER(b);
  }
  return a == b;
}

uint32_t hash_bits(uint64_t hash) {
  // from Wren,
  // adapted from http://web.archive.org/web/20071223173210/http://www.concentric.net/~Ttwang/tech/inthash.htm
  hash = ~hash + (hash << 18);
  hash = hash ^ (hash >> 31);
  hash = hash * 21;
  hash = hash ^ (hash >> 11);
  hash = hash + (hash << 6);
  hash = hash ^ (hash >> 22);
  return (uint32_t)(hash & 0x3fffffff);
}

uint32_t hash_string(const char* str, int len) {
  // FNV-1a hashing algorithm
  uint32_t hash = 2166136261u;
  uint32_t fnv_prime = 16777619u;
  for (int i = 0; i != len; i++) {
    hash = hash ^ (uint8_t)(str[i]);
    hash = hash * fnv_prime;
  }
  return hash;
}

uint32_t hash_object(Obj* obj) {
  switch (obj->type) {
    case OBJ_STR:
      return ((ObjString*)obj)->hash;
    default:
      UNREACHABLE("hash object");
  }
}

uint32_t hash_value(Value v) {
  if (IS_OBJ(v)) {
    return hash_object(AS_OBJ(v));
  }
  return hash_bits(v);
}

Obj* create_object(VM* vm, ObjTy ty, size_t size) {
  Obj* obj =
      vm_alloc(vm, NULL, 0, size, "allocation failed -- create_object");
  obj->type = ty;
  // link obj to vm's collection of allocated objects
  obj->next = vm->objects;
  vm->objects = obj;
  return obj;
}

int copy_str(VM* vm, const char* src, char** dest, int len) {
  int i, real_len;
  for (i = 0, real_len = 0; i < len; i++) {
    if (src[i] != '\\') {
      (*dest)[i] = src[i];
    } else {
      switch (src[i + 1]) {
        case '\\':
          (*dest)[i] = '\\';
          break;
        case '"':
          (*dest)[i] = '"';
          break;
        case '\'':
          (*dest)[i] = '\'';
          break;
        case 'r':
          (*dest)[i] = '\r';
          break;
        case 't':
          (*dest)[i] = '\t';
          break;
        case 'n':
          (*dest)[i] = '\n';
          break;
        case 'f':
          (*dest)[i] = '\f';
          break;
        case 'b':
          (*dest)[i] = '\b';
          break;
        case 'a':
          (*dest)[i] = '\a';
          break;
        default:
          // TODO: better error handling
          error("unknown escape sequence");
      }
      i++;
    }
    real_len++;
  }
  // TODO: necessary?
  if ((len - real_len) > 5) {
    // trim the string
    void* tmp = realloc(*dest, i);
    if (tmp) {
      FREE(vm, *dest, char);
      *dest = tmp;
    }
  }
  return real_len;
}

inline static int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}

Value create_string(VM* vm, Table* table, char* str, int len) {
  uint32_t hash = hash_string(str, len);
  ObjString* string = table_find_interned(table, str, len, hash);
  Value val;
  if (!string) {
    string = CREATE_OBJ(vm, ObjString, OBJ_STR, sizeof(ObjString));
    string->hash = hash;
    string->str = ALLOC(vm, char, len);
    string->len = copy_str(vm, str, &string->str, len);
    val = OBJ_VAL(string);
    table_put(table, vm, val, FALSE_VAL);
  } else {
    val = OBJ_VAL(string);
  }
  return val;
}

ObjList* create_list(VM* vm, int len) {
  int cap = GROW_CAPACITY(align_to(len, BUFFER_INIT_SIZE));
  ObjList* list = CREATE_OBJ(
      vm,
      ObjList,
      OBJ_LIST,
      sizeof(ObjList) + (sizeof(Value) * cap));
  list->elems.length = len;
  list->elems.capacity = cap;
  return list;
}
