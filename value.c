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
    case OBJ_HMAP:
      return "hashmap";
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
    case OBJ_HMAP: {
      ObjHashMap* map = AS_MAP(val);
      printf("#{");
      if (map->length) {
        HashEntry* entry;
        for (int i = 0, j = 0; i < map->capacity; i++) {
          entry = &map->entries[i];
          if (IS_NOTHING(entry->key)) {
            // skip deleted and fresh entries
            continue;
          }
          print_value(entry->key);
          printf(": ");
          print_value(entry->value);
          if (j < map->length - 1) {
            printf(", ");
          }
          j++;
        }
      }
      printf("}");
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
      string->len = copy_str_compact(vm, str, &string->str, len);
      string->str[string->len] = '\0';
    } else {
      string->str = str;
      string->len = len;
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
  uint32_t start_index = hash_value(key) & capacity;
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
        table->capacity);
  }
  FREE(vm, table->entries, HashEntry);
  table->entries = new_entries;
  table->capacity = capacity;
}

void hashmap_put(ObjHashMap* table, VM* vm, Value key, Value value) {
  if (table->length >= table->capacity * LOAD_FACTOR) {
    rehash(table, vm);
  }
  if (insert_entry(table->entries, key, value, table->capacity)) {
    table->length++;
  }
}

Value hashmap_get(ObjHashMap* table, Value key) {
  HashEntry* entry;
  if (find_entry(table->entries, table->capacity, &entry, key)) {
    return entry->value;
  }
  return NOTHING_VAL;
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
      if (string->len == len && string->hash == hash
          && memcmp(string->str, str, len) == 0) {
        return string;
      }
    }
    index = (index + 1) & capacity;
  } while (start_index != index);
  return NULL;
}
