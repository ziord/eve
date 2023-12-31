#include "value.h"

#include "map.h"
#include "vm.h"

void init_value_pool(ValuePool* vp) {
  vp->values = NULL;
  vp->length = 0;
  vp->capacity = 0;
}

void free_value_pool(ValuePool* vp, VM* vm) {
  FREE_BUFFER(vm, vp->values, Value, vp->capacity);
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
  FREE_BUFFER(vm, code->bytes, byte_t, code->capacity);
  FREE_BUFFER(vm, code->lines, int, code->capacity);
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
    case OBJ_STRUCT:
      return "struct";
    case OBJ_INSTANCE:
      return "instance";
    case OBJ_UPVALUE:
      return "upvalue";
    case OBJ_MODULE:
      return "module";
    case OBJ_CFN:
      return "builtin_function";
  }
  UNREACHABLE("unknown object type");
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
      return;
    }
    case OBJ_CLOSURE: {
      printf("{fn %s}", get_func_name(AS_CLOSURE(val)->func));
      return;
    }
    case OBJ_FN: {
      printf("{fn %s}", get_func_name(AS_FUNC(val)));
      return;
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
      return;
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
      return;
    }
    case OBJ_UPVALUE: {
      printf("{upvalue}");
      return;
    }
    case OBJ_STRUCT: {
      printf("{struct %s}", AS_STRUCT(val)->name->str);
      return;
    }
    case OBJ_MODULE: {
      printf("{module %s}", AS_STRUCT(val)->name->str);
      return;
    }
    case OBJ_INSTANCE: {
      printf("{instanceof %s}", AS_INSTANCE(val)->strukt->name->str);
      return;
    }
    case OBJ_CFN: {
      printf("{builtin_fn %s}", AS_CFUNC(val)->name);
      return;
    }
  }
  UNREACHABLE("print: unknown object type");
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

Value object_to_string(VM* vm, Value val) {
  switch (AS_OBJ(val)->type) {
    case OBJ_STR:
      return (val);
    case OBJ_LIST: {
      char buff[15];
      int len = snprintf(buff, 15, "@list[%d]", AS_LIST(val)->elems.length);
      return create_stringv(vm, &vm->strings, buff, len, false);
    }
    case OBJ_HMAP: {
      char buff[20];
      int len = snprintf(buff, 20, "@hashmap[%d]", AS_HMAP(val)->length);
      return create_stringv(vm, &vm->strings, buff, len, false);
    }
    case OBJ_CLOSURE: {
      ObjFn* fn = AS_CLOSURE(val)->func;
      int len = 10 + (fn->name ? fn->name->length : 3);
      char buff[len];
      len = snprintf(buff, len, "@fn[%s]", get_func_name(fn));
      return create_stringv(vm, &vm->strings, buff, len, false);
    }
    case OBJ_MODULE:
    case OBJ_STRUCT: {
      ObjString* name = AS_STRUCT(val)->name;
      int len = 10 + name->length;
      char buff[len];
      len = snprintf(
          buff,
          len,
          AS_OBJ(val)->type == OBJ_STRUCT ? "@struct[%s]" : "@module[%s]",
          name->str);
      return create_stringv(vm, &vm->strings, buff, len, false);
    }
    case OBJ_INSTANCE: {
      ObjString* name = AS_INSTANCE(val)->strukt->name;
      int len = 12 + name->length;
      char buff[len];
      len = snprintf(buff, len, "@instance[%s]", name->str);
      return create_stringv(vm, &vm->strings, buff, len, false);
    }
    case OBJ_CFN: {
      const char* name = AS_CFUNC(val)->name;
      int len = (int)strlen(name) + 14;
      char buff[len];
      len = snprintf(buff, len, "@builtin_fn[%s]", name);
      return create_stringv(vm, &vm->strings, buff, len, false);
    }
    case OBJ_FN:
    case OBJ_UPVALUE:
      break;
  }
  UNREACHABLE("object value to string");
}

Value value_to_string(VM* vm, Value val) {
  if (IS_OBJ(val)) {
    return object_to_string(vm, val);
  } else if (IS_NUMBER(val)) {
    double value = AS_NUMBER(val);
    if (isnan(value)) {
      return create_stringv(vm, &vm->strings, "nan", 3, false);
    } else if (isinf(value)) {
      if (value > 0) {
        return create_stringv(vm, &vm->strings, "inf", 3, false);
      } else {
        return create_stringv(vm, &vm->strings, "-inf", 3, false);
      }
    } else {
      char buff[25];
      int len = snprintf(buff, 25, "%.14g", AS_NUMBER(val));
      return create_stringv(vm, &vm->strings, buff, len, false);
    }
  } else if (IS_BOOL(val)) {
    int size;
    char* bol = AS_BOOL(val) ? (size = 4, "true") : (size = 5, "false");
    return create_stringv(vm, &vm->strings, bol, size, false);
  } else if (IS_NONE(val)) {
    return create_stringv(vm, &vm->strings, "None", 4, false);
  } else {
    UNREACHABLE("primitive value to string");
  }
}

/*********************
 *
 * > Object routines
 *
 ********************/

static uint32_t hash_string(const char* str, int len);

Obj* create_object(VM* vm, ObjTy ty, size_t size) {
#ifdef EVE_DEBUG_GC
  printf("  [*] allocate for type %d\n", ty);
#endif
  Obj* obj =
      vm_alloc(vm, NULL, 0, size, "allocation failed -- create_object");
  obj->type = ty;
  obj->marked = false;
  // link obj to vm's collection of allocated objects
  obj->next = vm->objects;
  vm->objects = obj;
  return obj;
}

ObjString*
create_string(VM* vm, Map* map, char* str, int len, bool is_alloc) {
  uint32_t hash = hash_string(str, len);
  ObjString* string = map_find_interned(map, str, len, hash);
  if (!string) {
    string = CREATE_OBJ(vm, ObjString, OBJ_STR, sizeof(ObjString));
    vm_push_stack(vm, OBJ_VAL(string));  // gc reasons
    string->str = str;  // gc reasons
    string->hash = hash;
    if (!is_alloc) {
      string->str = ALLOC(vm, char, len + 1);
      string->length = copy_str_compact(vm, str, &string->str, len);
      string->str[string->length] = '\0';
    } else {
      string->str = str;
      string->length = len;
      // track the already allocated bytes
      vm->gc.bytes_allocated += (len + 1);
    }
    map_put(map, vm, string, FALSE_VAL);
    vm_pop_stack(vm);  // gc reasons
  } else {
    if (is_alloc) {
      // cleanup, since we already have the
      // allocated string (and it's tracked)
      free(str);
    }
  }
  return string;
}

ObjString*
create_de_string(VM* vm, Map* map, char* str, int len, uint32_t hash) {
  ObjString* string = map_find_interned(map, str, len, hash);
  if (!string) {
    string = CREATE_OBJ(vm, ObjString, OBJ_STR, sizeof(ObjString));
    string->hash = hash;
    string->str = str;
    string->length = len;
    // track the already allocated bytes
    vm->gc.bytes_allocated += (len + 1);
    map_put(map, vm, string, FALSE_VAL);
  } else {
    // cleanup, since we already have the
    // allocated string (and it's tracked)
    free(str);
  }
  return string;
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
  fn->module = NULL;
  return fn;
}

ObjClosure* create_closure(VM* vm, ObjFn* func) {
  Env env = GROW_BUFFER(vm, NULL, ObjUpvalue*, 0, func->env_len);
  for (int i = 0; i < func->env_len; i++) {
    env[i] = NULL;
  }
  ObjClosure* closure =
      CREATE_OBJ(vm, ObjClosure, OBJ_CLOSURE, sizeof(ObjClosure));
  closure->env = env;
  closure->func = func;
  closure->env_len = func->env_len;
  return closure;
}

ObjUpvalue* create_upvalue(VM* vm, Value* location) {
  ObjUpvalue* upvalue =
      CREATE_OBJ(vm, ObjUpvalue, OBJ_UPVALUE, sizeof(ObjUpvalue));
  upvalue->next = NULL;
  upvalue->location = location;
  upvalue->value = NONE_VAL;
  return upvalue;
}

ObjStruct* create_struct(VM* vm, ObjString* name) {
  ObjStruct* strukt =
      CREATE_OBJ(vm, ObjStruct, OBJ_STRUCT, sizeof(ObjStruct));
  map_init(&strukt->fields);
  strukt->name = name;
  return strukt;
}

ObjStruct* create_module(VM* vm, ObjString* name) {
  ObjStruct* mod = create_struct(vm, name);
  mod->obj.type = OBJ_MODULE;
  return mod;
}

ObjInstance* create_instance(VM* vm, ObjStruct* strukt) {
  ObjInstance* instance =
      CREATE_OBJ(vm, ObjInstance, OBJ_INSTANCE, sizeof(ObjInstance));
  instance->strukt = strukt;
  map_init(&instance->fields);
  return instance;
}

ObjCFn* create_cfn(VM* vm, CFn fn, int arity, const char* name) {
  ObjCFn* func = CREATE_OBJ(vm, ObjCFn, OBJ_CFN, sizeof(ObjCFn));
  func->fn = fn;
  func->arity = arity;
  func->name = name;
  return func;
}

inline char* get_func_name(ObjFn* fn) {
  return fn->name ? fn->name->str : "<anonymous>";
}

inline static uint32_t hash_bits(uint64_t hash) {
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
      return fn->name->hash ^ hash_bits(fn->arity)
          ^ hash_bits(fn->code.length);
    }
    case OBJ_CFN: {
      ObjCFn* fn = (ObjCFn*)obj;
      int len = (int)strlen(fn->name);
      return hash_string(fn->name, len) ^ hash_bits(fn->arity);
    }
    case OBJ_MODULE:
    case OBJ_STRUCT: {
      ObjStruct* strukt = ((ObjStruct*)obj);
      return strukt->name->hash ^ hash_bits(strukt->fields.length);
    }
    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)obj;
      return instance->strukt->name->hash
          ^ hash_bits(instance->fields.length);
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
  FREE_BUFFER(vm, table->entries, HashEntry, table->capacity);
  table->entries = new_entries;
  table->capacity = capacity;
}

bool hashmap_put(ObjHashMap* table, VM* vm, Value key, Value value) {
  if (table->length >= table->capacity * MAP_LOAD_FACTOR) {
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

bool hashmap_has_key(ObjHashMap* table, Value key, Value* value) {
  HashEntry* entry;
  if (find_entry(table->entries, table->capacity, &entry, key)) {
    *value = entry->value;
    return true;
  }
  return false;
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

void hashmap_copy(VM* vm, ObjHashMap* map1, ObjHashMap* map2) {
  // copy map2 into map1
  if (!map1->capacity || !map2->capacity)
    return;
  HashEntry* entry;
  for (int i = 0; i < map2->capacity; i++) {
    entry = &map2->entries[i];
    if (!IS_NOTHING(entry->key)) {
      hashmap_put(map1, vm, entry->key, entry->value);
    }
  }
}

void hashmap_get_keys(ObjHashMap* table, ObjList* list) {
  HashEntry* entry;
  int index = 0;
  for (int i = 0; i < table->capacity; i++) {
    entry = &table->entries[i];
    if (!IS_NOTHING(entry->key)) {
      list->elems.buffer[index++] = entry->key;
    }
  }
}

void hashmap_init(ObjHashMap* table) {
  table->length = table->capacity = 0;
  table->entries = NULL;
}
