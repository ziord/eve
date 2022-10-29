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

inline static bool at_end(Lexer* lexer) {
  return (*lexer->current == '\0');
}

inline static bool is_alpha(char ch) {
  return ch == '_' || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
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

static TokenTy
expect(Lexer* lexer, char* rem, int start, int rest, TokenTy ty) {
  if (lexer->current - lexer->start == start + rest) {
    if (memcmp(lexer->start + start, rem, rest) == 0) {
      return ty;
    }
  }
  return TK_IDENT;
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

TokenTy keyword_type(Lexer* lexer, char ch) {
  // keywords:
  // true, false, None, struct, while,
  // return, if, else, fn, let, show
  switch (ch) {
    case 't':
      return expect(lexer, "rue", 1, 3, TK_TRUE);
    case 'f':
      switch (*(lexer->start + 1)) {
        case 'a':
          return expect(lexer, "lse", 2, 3, TK_FALSE);
      }
    case 'N':
      return expect(lexer, "one", 1, 3, TK_NONE);
    case 's':
    case 'w':
    case 'r':
    case 'i':
    case 'e':
    case 'l':
    default:
      break;
  }
  return TK_IDENT;
}

Token lex_ident(Lexer* lexer, char ch) {
  while (is_alpha(PEEK(lexer))) {
    advance(lexer);
  }
  return new_token(lexer, keyword_type(lexer, ch));
}

Token lex_string(Lexer* lexer, char start) {
  while (!at_end(lexer)) {
    advance(lexer);
    if (PEEK(lexer) == '\\') {
      // we inspect this when we "create" the string
      advance(lexer);
    } else if (PEEK(lexer) == start) {
      advance(lexer);
      if (peek(lexer, -2) != '\\') {
        break;
      }
      continue;
    }
  }
  if (*(lexer->current - 1) != start) {
    return error_token(lexer, "Unclosed string");
  }
  return new_token(lexer, TK_STRING);
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
  } else if (is_alpha(ch)) {
    return lex_ident(lexer, ch);
  }
  switch (ch) {
    case '"':
      return lex_string(lexer, ch);
    case '+':
      return new_token(lexer, TK_PLUS);
    case '-':
      return new_token(lexer, TK_MINUS);
    case '/':
      return new_token(lexer, TK_FSLASH);
    case '~':
      return new_token(lexer, TK_TILDE);
    case '%':
      return new_token(lexer, TK_PERCENT);
    case '^':
      return new_token(lexer, TK_CARET);
    case '*':
      return new_token(lexer, check(lexer, '*') ? TK_STAR_STAR : TK_STAR);
    case '=':
      return new_token(lexer, check(lexer, '=') ? TK_EQ_EQ : TK_EQ);
    case '&':
      return new_token(lexer, check(lexer, '&') ? TK_AMP_AMP : TK_AMP);
    case '|':
      return new_token(lexer, check(lexer, '|') ? TK_PIPE_PIPE : TK_PIPE);
    case '!':
      return new_token(lexer, check(lexer, '=') ? TK_NOT_EQ : TK_EXC_MARK);
    case '>':
      return new_token(
          lexer,
          check(lexer, '>')       ? TK_RSHIFT
              : check(lexer, '=') ? TK_GRT_EQ
                                  : TK_GRT);
    case '<':
      return new_token(
          lexer,
          check(lexer, '<')       ? TK_LSHIFT
              : check(lexer, '=') ? TK_LESS_EQ
                                  : TK_LESS);
    default:
      return error_token(lexer, "unknown token type");
  }
}
