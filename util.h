#ifndef EVE_UTIL_H
#define EVE_UTIL_H
#include "common.h"

_Noreturn void error(char* fmt, ...);
#define UNREACHABLE(msg) \
  error( \
      "Reached unreachable\n@file: %s\n@line: %d\n@msg: %s\n", \
      __FILE__, \
      __LINE__, \
      msg)

#endif  //EVE_UTIL_H
