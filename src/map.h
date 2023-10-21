#ifndef EVE_MAP_H
#define EVE_MAP_H
/// specialized table for handling cases where a hashmap is desired
/// but keys are only strings.
#include "value.h"

void map_init(Map* map);
ObjString* map_find_interned(Map* map, char* str, int len, uint32_t hash);
bool map_has_key(Map* map, ObjString* key, Value* value);
bool map_remove(Map* map, ObjString* key);
Value map_get(Map* map, ObjString* key);
bool map_put(Map* map, VM* vm, ObjString* key, Value value);
void map_copy(VM* vm, Map* map1, Map* map2);
#endif  //EVE_MAP_H
