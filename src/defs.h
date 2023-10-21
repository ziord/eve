#ifndef EVE_DEFS_H
#define EVE_DEFS_H

#include <stdint.h>

// constant
#define CONST_MAX UINT8_MAX

// debug
#ifdef EVE_DEBUG
  #define EVE_DEBUG_GC
  #define EVE_DEBUG_EXECUTION
#endif
#ifdef EVE_DEBUG_GC
  #define EVE_DEBUG_STRESS_GC
#endif

// utils
#define QUOTE(str) #str
#define EXPAND_AND_QUOTE(str) QUOTE(str)
#define CAST(ty, val) ((ty)(val))

// version
#define EVE_VERSION_MAJOR 0
#define EVE_VERSION_MINOR 1
#define EVE_VERSION_PATCH 0
#define EVE_VERSION \
  EXPAND_AND_QUOTE(EVE_VERSION_MAJOR.EVE_VERSION_MINOR.EVE_VERSION_PATCH)

// optimizations
#define EVE_OPTIMIZE_IMPORTS

typedef struct VM VM;

#endif  //EVE_DEFS_H
