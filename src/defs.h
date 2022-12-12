#ifndef EVE_DEFS_H
#define EVE_DEFS_H

#include <stdint.h>

#define CONST_MAX UINT8_MAX
#define CAST(ty, val) ((ty)(val))

#ifdef EVE_DEBUG
  #define EVE_DEBUG_GC
  #define EVE_DEBUG_EXECUTION
#endif
#ifdef EVE_DEBUG_GC
  #define EVE_DEBUG_STRESS_GC
#endif

typedef struct VM VM;

#endif  //EVE_DEFS_H
