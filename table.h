#ifndef EVE_TABLE_H
#define EVE_TABLE_H
#include "value.h"

#define LOAD_FACTOR (0.75)

typedef struct {
  Value key;
  Value value;
} TEntry;

typedef struct {
  TEntry* entries;
  int length;
  int capacity;
} Table;

void init_table(Table* table);
void table_put(Table* table, VM* vm, Value key, Value value);
Value table_get(Table* table, Value key);
bool table_remove(Table* table, Value key);
ObjString*
table_find_interned(Table* table, char* str, int len, uint32_t hash);

#endif  //EVE_TABLE_H
