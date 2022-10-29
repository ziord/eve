#ifndef EVE_LEXER_H
#define EVE_LEXER_H
#include <ctype.h>
#include <string.h>

#include "common.h"

// clang-format off
typedef enum {
  TK_NUM,           // 1 | 3.5
  TK_PLUS,          // +
  TK_MINUS,         // -
  TK_STAR,          // *
  TK_STAR_STAR,     // **
  TK_EXC_MARK,      // !
  TK_TILDE,         // ~
  TK_FSLASH,        // /
  TK_PERCENT,       // %
  TK_GRT,           // <
  TK_LESS,          // >
  TK_EQ,            // =
  TK_AMP,           // &
  TK_CARET,         // ^
  TK_PIPE,          // |
  TK_PIPE_PIPE,     // ||
  TK_AMP_AMP,       // &&
  TK_GRT_EQ,        // >=
  TK_LESS_EQ,       // <=
  TK_NOT_EQ,        // !=
  TK_EQ_EQ,         // ==
  TK_LSHIFT,        // >>
  TK_RSHIFT,        // <<
  TK_FALSE,         // false
  TK_TRUE,          // true
  TK_NONE,          // None
  TK_IDENT,         // identifier
  TK_STRING,        // "..."
  TK_EOF,           // EOF
  TK_ERROR,         // <error>
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
