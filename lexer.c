#include "lexer.h"

#define PEEK(_lex) peek((_lex), 0)
static inline char get_char(Lexer* lexer);

void init_lexer(Lexer* lexer, char* src) {
  lexer->src = src;
  lexer->column = lexer->line = 1;
  lexer->start = lexer->current = src;
}

Token new_token(Lexer* lexer, TokenTy type) {
  Token token = {
      .ty = type,
      .line = lexer->line,
      .column = lexer->column,
      .value = lexer->start,
      .length = (int)(lexer->current - lexer->start)};
  return token;
}

Token error_token(Lexer* lexer, char* msg) {
  Token tok = new_token(lexer, TK_ERROR);
  tok.value = msg;
  return tok;
}

bool at_end(Lexer* lexer) {
  return (*lexer->current == '\0');
}

char peek(Lexer* lexer, int next) {
  if (at_end(lexer)) {
    return '\0';
  }
  return (*(lexer->current + next));
}

inline static void advance(Lexer* lexer) {
  if (*lexer->current == '\n') {
    lexer->column = 1;
    lexer->line++;
  } else {
    lexer->column++;
  }
  lexer->current++;
}

inline char get_char(Lexer* lexer) {
  advance(lexer);
  return *(lexer->current - 1);
}

inline static bool check(Lexer* lexer, char ch) {
  if (PEEK(lexer) == ch) {
    advance(lexer);
    return true;
  }
  return false;
}

void skip_whitespace(Lexer* lexer) {
  for (;;) {
    switch (PEEK(lexer)) {
      case ' ':
      case '\r':
      case '\t':
      case '\n':
        advance(lexer);
        break;
      default:
        return;
    }
  }
}

Token lex_num(Lexer* lexer, char start) {
  // support hex, double (regular, exponent)
  while (isdigit(PEEK(lexer))) {
    advance(lexer);
  }
  char curr = PEEK(lexer);
  bool is_hex = start == '0' && tolower(curr) == 'x';
  bool is_exp = !is_hex && tolower(curr) == 'e';
  bool is_dec = curr == '.';
  if (is_dec || is_exp || is_hex) {
    advance(lexer);
    // treat e[+|-]?
    if (is_exp && (PEEK(lexer) == '+' || PEEK(lexer) == '-')) {
      advance(lexer);
    }
    if (is_dec || is_exp) {
      while (isdigit(PEEK(lexer))) {
        advance(lexer);
      }
    } else {
      while (isxdigit(PEEK(lexer))) {
        advance(lexer);
      }
    }
  }
  return new_token(lexer, TK_NUM);
}

Token get_token(Lexer* lexer) {
  skip_whitespace(lexer);
  lexer->start = lexer->current;
  if (at_end(lexer)) {
    return new_token(lexer, TK_EOF);
  }
  char ch = get_char(lexer);
  if (isdigit(ch)) {
    return lex_num(lexer, ch);
  }
  switch (ch) {
    case '+':
      return new_token(lexer, TK_PLUS);
    case '-':
      return new_token(lexer, TK_MINUS);
    case '/':
      return new_token(lexer, TK_F_SLASH);
    case '*':
      return new_token(lexer, check(lexer, '*') ? TK_STAR_STAR : TK_STAR);
    case '!':
      return new_token(lexer, TK_EXC_MARK);
    case '~':
      return new_token(lexer, TK_TILDE);
    case '%':
      return new_token(lexer, TK_PERCENT);
    default:
      return error_token(lexer, "unknown token type");
  }
}
