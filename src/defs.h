#ifndef EVE_DEFS_H
#define EVE_DEFS_H

#include <stdint.h>

#define CONST_MAX UINT8_MAX
#define CAST(ty, val) ((ty)(val))
#define EVE_DEBUG_EXECUTION
#define EVE_DEBUG
#define EVE_DEBUG_GC
#define EVE_DEBUG_STRESS_GC
typedef struct VM VM;

#endif  //EVE_DEFS_H
