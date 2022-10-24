#include "errors.h"

#include "common.h"

// clang-format off
static Error errors[] = {
  [E000] = {.err_msg = "Lex error", .hlp_msg = NULL},
  [E001] = {.err_msg = "Expected token type '%s', but got '%s'", .hlp_msg = NULL},
  [E002] = {.err_msg = "Invalid token for prefix '%s'", .hlp_msg = NULL},
  [E003] = {.err_msg = "Invalid binary operator '%s'", .hlp_msg = NULL},
};
// clang-format on
