#include "errors.h"

#include "common.h"

// clang-format off
static Error errors[] = {
  [E000] = {.err_msg = "Lex error", .hlp_msg = NULL},
  [E001] = {.err_msg = "Expected token type '%s', but got '%s'", .hlp_msg = NULL},
  [E002] = {.err_msg = "Invalid token for prefix '%s'", .hlp_msg = NULL},
  [E003] = {.err_msg = "Invalid binary operator '%s'", .hlp_msg = NULL},
  [E004] = {.err_msg = "Maximum number of elements for list exceeded. [MAX='%s']", .hlp_msg = NULL},
  [E005] = {.err_msg = "Invalid hashmap key", .hlp_msg = "Keys should be expressions"},
  [E006] = {.err_msg = "Maximum number of items for map exceeded. [MAX='%s']", .hlp_msg = NULL},
  [E007] = {.err_msg = "Maximum number of arguments exceeded. [MAX='%s']", .hlp_msg = NULL},
};
// clang-format on
