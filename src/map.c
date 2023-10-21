#include "map.h"

/// map_get map_put map_remove map_has_key

void adjust_map(Map* map, VM* vm) {
  int new_cap = GROW_CAPACITY(map->capacity);
  MapEntry* entries = ALLOC(vm, MapEntry, new_cap);
  for (int i = 0; i < new_cap; i++) {
    entries[i].key = NULL;
    entries[i].value = NOTHING_VAL;
  }
  MapEntry* old_entries = map->entries;
  int old_capacity = map->capacity;
  map->entries = entries;
  map->capacity = new_cap;
  // store old entries
  for (int i = 0; i < old_capacity; i++) {
    if (old_entries[i].key != NULL) {
      map_put(map, vm, old_entries[i].key, old_entries[i].value);
    }
  }
  FREE_BUFFER(vm, old_entries, MapEntry, old_capacity);
}

bool map_put(Map* map, VM* vm, ObjString* key, Value value) {
  // check capacity
  if (map->length >= map->capacity * MAP_LOAD_FACTOR) {
    adjust_map(map, vm);
  }
  int cap = (map->capacity - 1);
  uint32_t index = key->hash & cap;
  MapEntry* entry;
  for (;;) {
    entry = &map->entries[index];
    if (entry->key == NULL) {
      entry->key = key;
      entry->value = value;
      map->length++;
      return true;
    } else if (entry->key == key) {
      entry->value = value;
      return false;
    }
    index = (index + 1) & cap;
  }
  UNREACHABLE("map_put");
}

Value map_get(Map* map, ObjString* key) {
  if (map->capacity == 0)
    return NOTHING_VAL;
  int cap = (map->capacity - 1);
  uint32_t index = key->hash & cap;
  MapEntry* entry;
  for (;;) {
    entry = &map->entries[index];
    if (entry->key == key) {
      return entry->value;
    } else if (entry->key == NULL) {
      return NOTHING_VAL;
    }
    index = (index + 1) & cap;
  }
  UNREACHABLE("map_get");
}

bool map_remove(Map* map, ObjString* key) {
  int cap = (map->capacity - 1);
  uint32_t index = key->hash & cap;
  MapEntry* entry;
  for (;;) {
    entry = &map->entries[index];
    if (entry->key == key) {
      entry->key = NULL;
      entry->value = NOTHING_VAL;
      map->length--;
      return true;
    } else if (entry->key == NULL) {
      return false;
    }
    index = (index + 1) & cap;
  }
  UNREACHABLE("map_remove");
}

bool map_has_key(Map* map, ObjString* key, Value* value) {
  if (map->capacity == 0)
    return false;
  int cap = (map->capacity - 1);
  uint32_t index = key->hash & cap;
  MapEntry* entry;
  for (;;) {
    entry = &map->entries[index];
    if (entry->key == key) {
      *value = entry->value;
      return true;
    } else if (entry->key == NULL) {
      return false;
    }
    index = (index + 1) & cap;
  }
  UNREACHABLE("map_has_key");
}

ObjString* map_find_interned(Map* map, char* str, int len, uint32_t hash) {
  if (map->capacity == 0)
    return NULL;
  uint32_t cap = map->capacity - 1;
  uint32_t index = hash & cap;
  MapEntry* entry;
  for (;;) {
    entry = &map->entries[index];
    if (entry->key == NULL) {
      return NULL;
    } else {
      ObjString* string = entry->key;
      if (string->length == len && string->hash == hash
          && memcmp(string->str, str, len) == 0) {
        return string;
      }
    }
    index = (index + 1) & cap;
  }
  UNREACHABLE("map_find_interned");
}

void map_copy(VM* vm, Map* map1, Map* map2) {
  // copy map2 into map1
  if (!map1->capacity || !map2->capacity)
    return;
  MapEntry* entry;
  for (int i = 0; i < map2->capacity; i++) {
    entry = &map2->entries[i];
    if (entry->key) {
      map_put(map1, vm, entry->key, entry->value);
    }
  }
}

void map_init(Map* map) {
  map->entries = NULL;
  map->length = map->capacity = 0;
}