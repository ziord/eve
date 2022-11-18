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

void init_code(Code* code) {
  code->lines = NULL;
  code->bytes = NULL;
  code->length = 0;
  code->capacity = 0;
  init_value_pool(&code->vpool);
}

void free_code(Code* code, VM* vm) {
  FREE(vm, code->bytes, byte_t);
  FREE(vm, code->lines, int);
  free_value_pool(&code->vpool, vm);
  init_code(code);
}

void write_code(Code* code, byte_t byte, int line, VM* vm) {
  if (code->length >= code->capacity) {
    code->capacity = GROW_CAPACITY(code->capacity);
    code->bytes =
        GROW_BUFFER(vm, code->bytes, byte_t, code->length, code->capacity);
    code->lines =
        GROW_BUFFER(vm, code->lines, int, code->length, code->capacity);
  }
  code->bytes[code->length] = byte;
  code->lines[code->length++] = line;
}

char* get_object_type(Obj* obj) {
  switch (obj->type) {
    case OBJ_STR:
      return "string";
    case OBJ_LIST:
      return "list";
    case OBJ_HMAP:
      return "hashmap";
    case OBJ_CLOSURE:
    case OBJ_FN:
      return "function";
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
  } else if (IS_NONE(val)) {
    return "None";
  } else {
    UNREACHABLE("unknown value type");
  }
}

void print_object(Value val, Obj* obj) {
  switch (obj->type) {
    case OBJ_STR: {
      printf("%s", AS_STRING(val)->str);
      break;
    }
    case OBJ_CLOSURE: {
      printf("{fn %s}", get_func_name(AS_CLOSURE(val)->func));
      break;
    }
    case OBJ_FN: {
      printf("{fn %s}", get_func_name(AS_FUNC(val)));
      break;
    }
    case OBJ_LIST: {
      ObjList* list = AS_LIST(val);
      Value elem;
      printf("[");
      for (int i = 0; i < list->elems.length; i++) {
        elem = list->elems.buffer[i];
        if (elem != val) {
          if (!IS_STRING(elem)) {
            print_value(elem);
          } else {
            printf("\"");
            print_object(elem, AS_OBJ(elem));
            printf("\"");
          }
        } else {
          printf("[...]");
        }
        if (i < list->elems.length - 1) {
          printf(", ");
        }
      }
      printf("]");
      break;
    }
    case OBJ_HMAP: {
      ObjHashMap* map = AS_HMAP(val);
      printf("#{");
      if (map->length) {
        HashEntry* entry;
        for (int i = 0, j = 0; i < map->capacity; i++) {
          entry = &map->entries[i];
          if (IS_NOTHING(entry->key)) {
            // skip deleted and fresh entries
            continue;
          }
          // print key
          if (IS_STRING(entry->key)) {
            printf("\"");
            print_object(entry->key, AS_OBJ(entry->key));
            printf("\"");
          } else {
            print_value(entry->key);
          }
          printf(": ");
          // print value
          if (entry->value != val) {
            if (IS_STRING(entry->value)) {
              printf("\"");
              print_object(entry->value, AS_OBJ(entry->value));
              printf("\"");
            } else {
              print_value(entry->value);
            }
          } else {
            printf("#{...}");
          }
          if (j < map->length - 1) {
            printf(", ");
          }
          j++;
        }
      }
      printf("}");
      break;
    }
    case OBJ_UPVALUE: {
      printf("{upvalue}");
      break;
    }
    case OBJ_STRUCT: {
      printf("{struct %s}", AS_STRUCT(val)->name->str);
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
    printf("%.14g", AS_NUMBER(val));
  } else if (IS_NONE(val)) {
    printf("None");
  } else if (IS_NOTHING(val)) {
    // only used in debug mode.
    printf("<Nothing>");
  } else {
    UNREACHABLE("print: unknown value type");
  }
}

bool value_falsy(Value v) {
  return (
      IS_NUMBER(v) && !AS_NUMBER(v) || (IS_BOOL(v) && !AS_BOOL(v))
      || IS_NONE(v) || (IS_LIST(v) && !AS_LIST(v)->elems.length)
      || (IS_STRING(v) && !AS_STRING(v)->length)
      || (IS_HMAP(v) && !AS_HMAP(v)->length));
}

bool value_equal(Value a, Value b) {
  if (IS_NUMBER(a) && IS_NUMBER(b)) {
    return AS_NUMBER(a) == AS_NUMBER(b);
  }
  return a == b;
}

Value object_to_string(VM* vm, Value val) {
  switch (AS_OBJ(val)->type) {
    case OBJ_STR:
      return (val);
    case OBJ_LIST: {
      char buff[15];
      int len = snprintf(buff, 15, "@list[%d]", AS_LIST(val)->elems.length);
      return (create_string(vm, &vm->strings, buff, len, false));
    }
    case OBJ_HMAP: {
      char buff[20];
      int len = snprintf(buff, 20, "@hashmap[%d]", AS_HMAP(val)->length);
      return (create_string(vm, &vm->strings, buff, len, false));
    }
    case OBJ_CLOSURE: {
      ObjFn* fn = AS_CLOSURE(val)->func;
      int len = 10 + (fn->name ? fn->name->length : 3);
      char buff[len];
      len = snprintf(buff, len, "@fn[%s]", get_func_name(fn));
      return (create_string(vm, &vm->strings, buff, len, false));
    }
    case OBJ_STRUCT: {
      ObjString* name = AS_STRUCT(val)->name;
      int len = 10 + name->length;
      char buff[len];
      len = snprintf(buff, len, "@struct[%s]", name->str);
      return (create_string(vm, &vm->strings, buff, len, false));
    }
    default:
      UNREACHABLE("object value to string");
  }
}

Value value_to_string(VM* vm, Value val) {
  if (IS_OBJ(val)) {
    return object_to_string(vm, val);
  } else if (IS_NUMBER(val)) {
    double value = AS_NUMBER(val);
    if (isnan(value)) {
      return (create_string(vm, &vm->strings, "nan", 3, false));
    } else if (isinf(value)) {
      if (value > 0) {
        return (create_string(vm, &vm->strings, "inf", 3, false));
      } else {
        return (create_string(vm, &vm->strings, "-inf", 3, false));
      }
    } else {
      char buff[25];
      int len = snprintf(buff, 25, "%g", AS_NUMBER(val));
      return (create_string(vm, &vm->strings, buff, len, false));
    }
  } else if (IS_BOOL(val)) {
    int size;
    char* bol = AS_BOOL(val) ? (size = 4, "true") : (size = 5, "false");
    return (create_string(vm, &vm->strings, bol, size, false));
  } else if (IS_NONE(val)) {
    return (create_string(vm, &vm->strings, "None", 4, false));
  } else {
    UNREACHABLE("primitive value to string");
  }
}

void free_object(VM* vm, Obj* obj) {
  switch (obj->type) {
    case OBJ_STR: {
      ObjString* st = (ObjString*)obj;
      FREE_BUFFER(vm, st->str, char, st->length + 1);
      FREE(vm, st, ObjString);
      break;
    }
    case OBJ_LIST: {
      ObjList* list = (ObjList*)obj;
      size_t size =
          sizeof(ObjList) + (sizeof(Value) * list->elems.capacity);
      FREE_FLEX(vm, list, size);
      break;
    }
    case OBJ_HMAP: {
      ObjHashMap* map = (ObjHashMap*)obj;
      FREE_BUFFER(vm, map->entries, HashEntry, map->capacity);
      FREE(vm, map, ObjHashMap);
      break;
    }
    case OBJ_FN: {
      ObjFn* func = (ObjFn*)obj;
      free_code(&func->code, vm);
      FREE(vm, func, ObjFn);
      break;
    }
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)obj;
      FREE_BUFFER(vm, closure->env, ObjUpvalue*, closure->env_len);
      FREE(vm, closure, ObjClosure);
      break;
    }
    case OBJ_UPVALUE: {
      FREE(vm, obj, ObjUpvalue);
      break;
    }
    case OBJ_STRUCT: {
      FREE(vm, obj, ObjStruct);
      break;
    }
  }
}

/*********************
 *
 * > Object routines
 *
 ********************/

static uint32_t hash_string(const char* str, int len);

Obj* create_object(VM* vm, ObjTy ty, size_t size) {
  Obj* obj =
      vm_alloc(vm, NULL, 0, size, "allocation failed -- create_object");
  obj->type = ty;
  // link obj to vm's collection of allocated objects
  obj->next = vm->objects;
  vm->objects = obj;
  return obj;
}

Value create_string(
    VM* vm,
    ObjHashMap* table,
    char* str,
    int len,
    bool is_alloc) {
  uint32_t hash = hash_string(str, len);
  ObjString* string = hashmap_find_interned(table, str, len, hash);
  Value val;
  if (!string) {
    string = CREATE_OBJ(vm, ObjString, OBJ_STR, sizeof(ObjString));
    string->hash = hash;
    if (!is_alloc) {
      string->str = ALLOC(vm, char, len + 1);
      string->length = copy_str_compact(vm, str, &string->str, len);
      string->str[string->length] = '\0';
    } else {
      string->str = str;
      string->length = len;
      // track the already allocated bytes
      vm->bytes_alloc += len;
    }
    val = OBJ_VAL(string);
    hashmap_put(table, vm, val, FALSE_VAL);
  } else {
    val = OBJ_VAL(string);
    if (is_alloc) {
      // cleanup, since we already have the
      // allocated string (and it's tracked)
      free(str);
    }
  }
  return val;
}

ObjList* create_list(VM* vm, int len) {
  int cap = GROW_CAPACITY(ALIGN_TO(len, BUFFER_INIT_SIZE));
  ObjList* list = CREATE_OBJ(
      vm,
      ObjList,
      OBJ_LIST,
      sizeof(ObjList) + (sizeof(Value) * cap));
  list->elems.length = len;
  list->elems.capacity = cap;
  return list;
}

ObjHashMap* create_hashmap(VM* vm) {
  ObjHashMap* hm = CREATE_OBJ(vm, ObjHashMap, OBJ_HMAP, sizeof(ObjHashMap));
  hashmap_init(hm);
  return hm;
}

ObjFn* create_function(VM* vm) {
  ObjFn* fn = CREATE_OBJ(vm, ObjFn, OBJ_FN, sizeof(ObjFn));
  init_code(&fn->code);
  fn->arity = 0;
  fn->env_len = 0;
  fn->name = NULL;
  return fn;
}

ObjClosure* create_closure(VM* vm, ObjFn* func) {
  ObjClosure* closure =
      CREATE_OBJ(vm, ObjClosure, OBJ_CLOSURE, sizeof(ObjClosure));
  if (func->env_len) {
    closure->env = GROW_BUFFER(vm, NULL, ObjUpvalue*, 0, func->env_len);
    for (int i = 0; i < func->env_len; i++) {
      closure->env[i] = NULL;
    }
  }
  closure->func = func;
  closure->env_len = func->env_len;
  return closure;
}

ObjUpvalue* create_upvalue(VM* vm, Value* location) {
  ObjUpvalue* upvalue =
      CREATE_OBJ(vm, ObjUpvalue, OBJ_UPVALUE, sizeof(ObjUpvalue));
  upvalue->next = NULL;
  upvalue->location = location;
  return upvalue;
}

ObjStruct* create_struct(VM* vm, ObjString* name) {
  ObjStruct* strukt =
      CREATE_OBJ(vm, ObjStruct, OBJ_STRUCT, sizeof(ObjStruct));
  hashmap_init(&strukt->fields);
  strukt->name = name;
  return strukt;
}

inline char* get_func_name(ObjFn* fn) {
  return fn->name ? fn->name->str : "<anonymous>";
}

static uint32_t hash_bits(uint64_t hash) {
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

static uint32_t hash_string(const char* str, int len) {
  // FNV-1a hashing algorithm
  uint32_t hash = 2166136261u;
  uint32_t fnv_prime = 16777619u;
  for (int i = 0; i < len; i++) {
    hash = hash ^ (uint8_t)(str[i]);
    hash = hash * fnv_prime;
  }
  return hash;
}

static uint32_t hash_object(Obj* obj) {
  switch (obj->type) {
    case OBJ_STR:
      return ((ObjString*)obj)->hash;
    case OBJ_CLOSURE: {
      ObjFn* fn = ((ObjClosure*)obj)->func;
      return hash_bits(fn->arity) ^ hash_bits(fn->code.length);
    }
    default:
      // TODO: better error handling
      error("Unhashable type '%s'", get_object_type(obj));
  }
}

static uint32_t hash_value(Value v) {
  if (IS_OBJ(v)) {
    return hash_object(AS_OBJ(v));
  }
  return hash_bits(v);
}

static bool
find_entry(HashEntry* entries, int capacity, HashEntry** slot, Value key) {
  // return true if we find an entry matching key, else false
  if (capacity == 0)
    return false;
  int cap = capacity - 1;
  uint32_t start_index = hash_value(key) & cap;
  uint32_t index = start_index;
  HashEntry *deleted = NULL, *entry;
  do {
    entry = &entries[index];
    // insert
    if (IS_NOTHING(entry->key)) {
      // check if it's a fresh entry, by checking if the value is NOTHING
      if (IS_NOTHING(entry->value)) {
        *slot = deleted ? deleted : entry;
        return false;
      } else if (deleted == NULL) {
        // -- deleted entries have values equal to NONE
        // -- so we set 'deleted' for reuse
        deleted = entry;
      }
    } else if (value_equal(entry->key, key)) {
      *slot = entry;
      return true;
    }
    index = (index + 1) & cap;
  } while (index != start_index);
  // at this point, the table is full of deleted entries.
  ASSERT(deleted, "table must have at least one deleted slot");
  *slot = deleted;
  return false;
}

static bool
insert_entry(HashEntry* entries, Value key, Value value, int capacity) {
  ASSERT(capacity && entries, "entries and capacity should be non-empty");
  HashEntry* entry;
  if (find_entry(entries, capacity, &entry, key)) {  // update
    entry->value = value;
    return false;
  } else {  // insert
    entry->key = key;
    entry->value = value;
    return true;
  }
}

static void rehash(ObjHashMap* table, VM* vm) {
  int capacity = GROW_CAPACITY(table->capacity);
  HashEntry* new_entries = ALLOC(vm, HashEntry, capacity);
  for (int i = 0; i < capacity; i++) {
    // initialize new entries
    new_entries[i].key = NOTHING_VAL;
    new_entries[i].value = NOTHING_VAL;
  }
  for (int i = 0; i < table->capacity; i++) {
    // don't copy empty or deleted entries
    if (IS_NOTHING(table->entries[i].key))
      continue;
    insert_entry(
        new_entries,
        table->entries[i].key,
        table->entries[i].value,
        capacity);
  }
  FREE(vm, table->entries, HashEntry);
  table->entries = new_entries;
  table->capacity = capacity;
}

bool hashmap_put(ObjHashMap* table, VM* vm, Value key, Value value) {
  if (table->length >= table->capacity * LOAD_FACTOR) {
    rehash(table, vm);
  }
  if (insert_entry(table->entries, key, value, table->capacity)) {
    table->length++;
    return true;
  }
  return false;
}

Value hashmap_get(ObjHashMap* table, Value key) {
  HashEntry* entry;
  if (find_entry(table->entries, table->capacity, &entry, key)) {
    return entry->value;
  }
  return NOTHING_VAL;
}

bool hashmap_has_key(ObjHashMap* table, Value key) {
  HashEntry* entry;
  return find_entry(table->entries, table->capacity, &entry, key);
}

bool hashmap_remove(ObjHashMap* table, Value key) {
  HashEntry* entry;
  if (find_entry(table->entries, table->capacity, &entry, key)) {
    entry->key = NOTHING_VAL;
    entry->value = NONE_VAL;  // deleted flag
    table->length--;
    return true;
  }
  return false;
}

void hashmap_init(ObjHashMap* table) {
  table->length = table->capacity = 0;
  table->entries = NULL;
}

ObjString* hashmap_find_interned(
    ObjHashMap* table,
    char* str,
    int len,
    uint32_t hash) {
  if (table->capacity == 0)
    return NULL;
  uint32_t capacity = table->capacity - 1;
  uint32_t index = hash & capacity;
  uint32_t start_index = index;
  HashEntry* entry;
  do {
    entry = &table->entries[index];
    if (IS_NOTHING(entry->key)) {
      // NONE value indicates deleted.
      // We intern strings with FALSE value, i.e. key -> string, value -> False_val
      if (IS_NONE(entry->value))
        return NULL;
    } else if (IS_STRING(entry->key)) {
      ObjString* string = AS_STRING(entry->key);
      if (string->length == len && string->hash == hash
          && memcmp(string->str, str, len) == 0) {
        return string;
      }
    }
    index = (index + 1) & capacity;
  } while (start_index != index);
  return NULL;
}
