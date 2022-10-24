#ifndef EVE_LEXER_H
#define EVE_LEXER_H
#include <ctype.h>

#include "common.h"

// clang-format off
typedef enum {
  TK_NUM,     // 1 | 3.5
  TK_PLUS,    // +
  TK_MINUS,   // -
  TK_STAR,    // *
  TK_F_SLASH, // /
  TK_EOF,     // EOF
  TK_ERROR,   // <error>
} TokenTy;
// clang-format on

typedef struct {
  TokenTy ty;
  int line;
  int length;
  int column;
  char* value;
} Token;

typedef struct {
  int column;
  int line;
  char *src, *start, *current;
} Lexer;

void init_lexer(Lexer* lexer, char* src);
Token get_token(Lexer* lexer);

#endif  //EVE_LEXER_H
