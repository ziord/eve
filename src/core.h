#ifndef EVE_CORE_H
#define EVE_CORE_H

#include "value.h"

void init_builtins(VM* vm);
void init_module(VM* vm, ObjStruct* module);

#endif  //EVE_CORE_H
