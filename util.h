#ifndef EVE_UTIL_H
#define EVE_UTIL_H
#include "common.h"

#define FAIL(_pre, _msg) \
  error( \
      "%s\n@file: %s\n@line: %d\n@msg: %s\n", \
      (_pre), \
      __FILE__, \
      __LINE__, \
      (_msg))
#define UNREACHABLE(_msg) FAIL("Reached unreachable", _msg)
#define ASSERT(tst, msg) (!(tst) ? FAIL("Assertion Error", (msg)) : (void)0)

_Noreturn void error(char* fmt, ...);

#endif  //EVE_UTIL_H
