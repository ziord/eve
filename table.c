#include "table.h"

void init_table(Table* table) {
  table->length = table->capacity = 0;
  table->entries = NULL;
}

bool find_entry(TEntry* entries, int capacity, TEntry** slot, Value key) {
  // return true if we find an entry matching key, else false
  if (capacity == 0)
    return false;
  int cap = capacity - 1;
  uint32_t start_index = hash_value(key) & capacity;
  uint32_t index = start_index;
  TEntry *deleted = NULL, *entry;
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

bool insert_entry(TEntry* entries, Value key, Value value, int capacity) {
  ASSERT(capacity && entries, "entries and capacity should be non-empty");
  TEntry* entry;
  if (find_entry(entries, capacity, &entry, key)) {  // update
    entry->value = value;
    return false;
  } else {  // insert
    entry->key = key;
    entry->value = value;
    return true;
  }
}

void rehash(Table* table, VM* vm) {
  int capacity = GROW_CAPACITY(table->capacity);
  TEntry* new_entries = ALLOC(vm, TEntry, capacity);
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
  FREE(vm, table->entries, TEntry);
  table->entries = new_entries;
  table->capacity = capacity;
}

void table_put(Table* table, VM* vm, Value key, Value value) {
  if (table->length >= table->capacity * LOAD_FACTOR) {
    rehash(table, vm);
  }
  if (insert_entry(table->entries, key, value, table->capacity)) {
    table->length++;
  }
}

Value table_get(Table* table, Value key) {
  TEntry* entry;
  if (find_entry(table->entries, table->capacity, &entry, key)) {
    return entry->value;
  }
  return NOTHING_VAL;
}

bool table_remove(Table* table, Value key) {
  TEntry* entry;
  if (find_entry(table->entries, table->capacity, &entry, key)) {
    entry->key = NOTHING_VAL;
    entry->value = NONE_VAL;  // deleted flag
    table->length--;
    return true;
  }
  return false;
}

ObjString*
table_find_interned(Table* table, char* str, int len, uint32_t hash) {
  if (table->capacity == 0)
    return NULL;
  uint32_t capacity = table->capacity - 1;
  uint32_t index = hash & capacity;
  TEntry* entry;
  for (;;) {
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
  }
}
