#ifndef EVE_ERRORS_H
#define EVE_ERRORS_H

typedef enum {
  E000,  // no error
  E0001,
  E0002,
  E0003,
  E0004,
  E0005,
  E0006,
  E0007,
  E0008,
  E0009,
  E0010,
  E0011,
  E0012,
  E0013,
  E0014,
  E0015,
} ErrorTy;

typedef struct {
  char* err_msg;
  char* hlp_msg;
} Error;

#endif  //EVE_ERRORS_H
