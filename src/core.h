#ifndef EVE_CORE_H
#define EVE_CORE_H

#include "value.h"

void init_builtins(VM* vm, ObjStruct* current_mod);
void inject_builtins(VM* vm, ObjStruct* module);

#endif  //EVE_CORE_H
