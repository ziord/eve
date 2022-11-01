#include "lexer.h"

#define PEEK(_lex) peek((_lex), 0)
static inline char get_char(Lexer* lexer);
char* token_types[] = {
    [TK_NUM] = "<number>",    [TK_PLUS] = "+",
    [TK_MINUS] = "-",         [TK_STAR] = "*",
    [TK_STAR_STAR] = "**",    [TK_EXC_MARK] = "!",
    [TK_TILDE] = "~",         [TK_FSLASH] = "/",
    [TK_PERCENT] = "%",       [TK_GRT] = ">",
    [TK_LESS] = "<",          [TK_EQ] = "=",
    [TK_AMP] = "&",           [TK_CARET] = "^",
    [TK_PIPE] = "|",          [TK_COMMA] = ",",
    [TK_LBRACK] = "(",        [TK_RBRACK] = ")",
    [TK_LSQ_BRACK] = "[",     [TK_RSQ_BRACK] = "]",
    [TK_LCURLY] = "{",        [TK_RCURLY] = "}",
    [TK_HASH] = "#",          [TK_COLON] = ":",
    [TK_SEMI_COLON] = ";",    [TK_PIPE_PIPE] = "||",
    [TK_AMP_AMP] = "&&",      [TK_GRT_EQ] = ">=",
    [TK_LESS_EQ] = "<=",      [TK_NOT_EQ] = "!=",
    [TK_EQ_EQ] = "==",        [TK_LSHIFT] = ">>",
    [TK_RSHIFT] = "<<",       [TK_FALSE] = "false",
    [TK_TRUE] = "true",       [TK_NONE] = "None",
    [TK_SHOW] = "show",       [TK_IDENT] = "<identifier>",
    [TK_STRING] = "<string>", [TK_EOF] = "<eof>",
    [TK_ERROR] = "<error>",
};

void init_lexer(Lexer* lexer, char* src) {
  lexer->src = src;
  lexer->column = lexer->line = 1;
  lexer->start = lexer->current = src;
  lexer->at_error = false;
}

Token new_token(Lexer* lexer, TokenTy type) {
  Token token = {
      .ty = type,
      .has_esc = false,
      .line = lexer->line,
      .column = lexer->column,
      .value = lexer->start,
      .error_ty = E000,
      .length = (int)(lexer->current - lexer->start)};
  return token;
}

Token error_token(Lexer* lexer, ErrorTy ty) {
  Token tok = new_token(lexer, TK_ERROR);
  tok.error_ty = ty;
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

char* get_src_at_line(Lexer* lexer, Token token, int* padding, int* len) {
  char* start = lexer->src;
  int line = 1;
  while (line != lexer->line) {
    start++;
    if (*start == '\n') {
      line++;
    }
  }
  start++;
  char* end = start;
  while (*end != '\n') {
    if (*end == '\0')
      break;
    end++;
  }
  *padding = token.column - token.length - 1;
  *len = end - start;
  return start;
}

void skip_line_comment(Lexer* lexer) {
  while (PEEK(lexer) != '\n') {
    if (at_end(lexer))
      return;
    advance(lexer);
  }
  advance(lexer);
}

void skip_multiline_comment(Lexer* lexer) {
  advance(lexer);  // skip '*'
  while (!at_end(lexer)) {
    if (PEEK(lexer) == '*' && peek(lexer, 1) == '#') {
      advance(lexer);  // skip '*'
      advance(lexer);  // skip '#'
      return;
    }
    advance(lexer);
  }
  if (at_end(lexer)) {
    lexer->at_error = true;
  }
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
      case '#':
        if (peek(lexer, 1) == '#') {
          skip_line_comment(lexer);
          break;
        } else if (peek(lexer, 1) == '*') {
          skip_multiline_comment(lexer);
          break;
        }
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
      switch (*(lexer->start + 1)) {
        case 'h':
          return expect(lexer, "ow", 2, 2, TK_SHOW);
      }
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
  bool has_esc = false;
  while (!at_end(lexer)) {
    if (PEEK(lexer) == '\\') {
      // we inspect this when we "create" the string
      advance(lexer);
      has_esc = true;
    } else if (PEEK(lexer) == start) {
      advance(lexer);
      if (peek(lexer, -2) != '\\') {
        break;
      }
    }
    advance(lexer);
  }
  if (*(lexer->current - 1) != start) {
    return error_token(lexer, E0001);
  }
  Token tok = new_token(lexer, TK_STRING);
  tok.has_esc = has_esc;
  return tok;
}

Token get_token(Lexer* lexer) {
  skip_whitespace(lexer);
  lexer->start = lexer->current;
  if (lexer->at_error) {
    // will only be reached if comment isn't closed.
    return error_token(lexer, E0002);
  } else if (at_end(lexer)) {
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
    case ',':
      return new_token(lexer, TK_COMMA);
    case ';':
      return new_token(lexer, TK_SEMI_COLON);
    case '+':
      return new_token(lexer, TK_PLUS);
    case '-':
      return new_token(lexer, TK_MINUS);
    case '(':
      return new_token(lexer, TK_LBRACK);
    case ')':
      return new_token(lexer, TK_RBRACK);
    case '[':
      return new_token(lexer, TK_LSQ_BRACK);
    case ']':
      return new_token(lexer, TK_RSQ_BRACK);
    case '#':
      return new_token(lexer, TK_HASH);
    case ':':
      return new_token(lexer, TK_COLON);
    case '{':
      return new_token(lexer, TK_LCURLY);
    case '}':
      return new_token(lexer, TK_RCURLY);
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
      return error_token(lexer, E0003);
  }
}
