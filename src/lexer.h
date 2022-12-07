#ifndef EVE_LEXER_H
#define EVE_LEXER_H
#include <ctype.h>
#include <string.h>

#include "common.h"
#include "errors.h"

// clang-format off
typedef enum {
  TK_NUM,           // 1 | 3.5
  TK_PLUS,          // +
  TK_MINUS,         // -
  TK_STAR,          // *
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
  TK_COMMA,         // ,
  TK_LBRACK,        // (
  TK_RBRACK,        // )
  TK_LSQ_BRACK,     // [
  TK_RSQ_BRACK,     // ]
  TK_LCURLY,        // {
  TK_RCURLY,        // }
  TK_HASH,          // #
  TK_AT,            // @
  TK_COLON,         // :
  TK_SEMI_COLON,    // ;
  TK_DOT,           // .
  TK_QMARK,         // ?
  TK_STAR_STAR,     // **
  TK_DCOLON,        // ::
  TK_PIPE_PIPE,     // ||
  TK_AMP_AMP,       // &&
  TK_GRT_EQ,        // >=
  TK_LESS_EQ,       // <=
  TK_NOT_EQ,        // !=
  TK_EQ_EQ,         // ==
  TK_LSHIFT,        // >>
  TK_RSHIFT,        // <<
  TK_ARROW,         // =>
  TK_FALSE,         // false
  TK_TRUE,          // true
  TK_NONE,          // None
  TK_SHOW,          // show
  TK_LET,           // let
  TK_TRY,           // try
  TK_THROW,         // throw
  TK_ASSERT,        // assert
  TK_IF,            // if
  TK_FN,            // fn
  TK_FOR,           // for
  TK_IN,            // in
  TK_ELSE,          // else
  TK_WHILE,         // while
  TK_BREAK,         // break
  TK_CONTINUE,      // continue
  TK_RETURN,        // return
  TK_STRUCT,        // struct
  TK_IDENT,         // identifier
  TK_STRING,        // "..."
  TK_EOF,           // EOF
  TK_ERROR,         // <error>
} TokenTy;
// clang-format on

typedef struct Token {
  TokenTy ty;
  ErrorTy error_ty;
  bool has_esc;
  int line;
  int length;
  int column;
  char* value;
} Token;

typedef struct {
  bool at_error;
  int column;
  int line;
  char *src, *start, *current;
} Lexer;

void init_lexer(Lexer* lexer, char* src);
char* get_src_at_line(Lexer* lexer, Token token, int* padding, int* len);
bool is_current_symbol(Lexer* lexer, char ch);
Token get_token(Lexer* lexer);
void rewind_state(Lexer* lexer, Token token);

#endif  //EVE_LEXER_H
