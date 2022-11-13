#include "errors.h"

#include "common.h"

// clang-format off
Error error_types[] = {
  [E000] = {.err_msg = "", .hlp_msg = NULL},
  [E0001] = {.err_msg = "Unclosed string", .hlp_msg = "Consider closing the comment with a \""},
  [E0002] = {.err_msg = "Unclosed comment", .hlp_msg = "Consider closing the comment with a *#"},
  [E0003] = {.err_msg = "Unknown token type", .hlp_msg = "The token found at this position was not recognized"},
  [E0004] = {.err_msg = "Token type mismatch", .hlp_msg = "Expected token type '%s', but got '%s'"},
  [E0005] = {.err_msg = "Invalid token for prefix: '%s'", .hlp_msg = "The token was found at an unexpected position"},
  [E0006] = {.err_msg = "Cannot use '%s' outside a loop statement", .hlp_msg = "Consider eliminating the statement"},
  [E0007] = {.err_msg = "Maximum number of elements for list exceeded.", .hlp_msg = "Max allowed is %d"},
  [E0008] = {.err_msg = "Cannot 'return' outside of a function", .hlp_msg = "Consider eliminating the return statement"},
  [E0009] = {.err_msg = "Maximum number of items for map exceeded.", .hlp_msg = "Max allowed is %d"},
  [E0010] = {.err_msg = "Maximum number of arguments exceeded.", .hlp_msg = "Max allowed is %d"},
  [E0011] = {.err_msg = "Duplicate argument '%.*s' in function definition.", .hlp_msg = "A function parameter's name must be unique"},
  [E0012] = {.err_msg = "Maximum number of parameters exceeded.", .hlp_msg = "Max allowed is %d"},
};
// clang-format on
