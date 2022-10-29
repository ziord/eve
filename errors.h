#ifndef EVE_ERRORS_H
#define EVE_ERRORS_H

typedef enum {
  E000,  // covers all lex error
  E001,
  E002,
  E003,
  E004,
} ErrorTy;

typedef struct {
  char* err_msg;
  char* hlp_msg;
} Error;

#endif  //EVE_ERRORS_H
