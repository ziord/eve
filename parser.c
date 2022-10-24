#include "parser.h"

typedef AstNode* (*PrefixFn)(Parser*);
typedef AstNode* (*InfixFn)(Parser*, AstNode*);

// clang-format off
typedef enum BindingPower {
  BP_NONE,        // other
  BP_ASSIGNMENT,  //
  BP_TERM,        // +, -
  BP_FACTOR,      // /, *, %
} BindingPower;
// clang-format on

typedef struct {
  BindingPower bp;
  PrefixFn prefix;
  InfixFn infix;
} ParseTable;

static AstNode* _parse(Parser* parser, BindingPower bp);
static AstNode* parse_num(Parser* parser);
static AstNode* parse_binary(Parser* parser, AstNode* left);

// clang-format off
ParseTable p_table[] = {
  [TK_NUM] = {.bp = BP_NONE, .prefix = parse_num, .infix = NULL},
  [TK_PLUS] = {.bp = BP_TERM, .prefix = NULL, .infix = parse_binary},
  [TK_MINUS] = {.bp = BP_TERM, .prefix = NULL, .infix = parse_binary},
  [TK_STAR] = {.bp = BP_FACTOR, .prefix = NULL, .infix = parse_binary},
  [TK_F_SLASH] = {.bp = BP_FACTOR, .prefix = NULL, .infix = parse_binary},
  [TK_EOF] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_ERROR] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
};
// clang-format on

Parser new_parser(char* src) {
  Lexer lexer;
  init_lexer(&lexer, src);
  Parser parser = {.lexer = lexer};
  init_store(&parser.store);
  return parser;
}

void parse_error(Parser* parser, Token tok, ErrorTy ty, char* msg) {
  error("Parse error: %s", tok.value);
}

void advance(Parser* parser) {
  parser->previous_tk = parser->current_tk;
  Token tok = get_token(&parser->lexer);
  if (tok.ty == TK_ERROR) {
    parse_error(parser, tok, E000, tok.value);
  } else {
    parser->current_tk = tok;
  }
}

void consume(Parser* parser, TokenTy ty) {
  if (parser->current_tk.ty == ty) {
    advance(parser);
  } else {
    parse_error(parser, parser->current_tk, E001, NULL);
  }
}

inline static bool is_tty(Parser* parser, TokenTy ty) {
  return parser->current_tk.ty == ty;
}

bool match(Parser* parser, TokenTy ty) {
  if (parser->current_tk.ty == ty) {
    advance(parser);
    return true;
  }
  return false;
}

static AstNode* _parse(Parser* parser, BindingPower bp) {
  PrefixFn pref = p_table[parser->current_tk.ty].prefix;
  if (pref == NULL) {
    parse_error(parser, parser->current_tk, E002, NULL);
  }
  AstNode* node = pref(parser);
  while (bp <= p_table[parser->current_tk.ty].bp) {
    InfixFn inf = p_table[parser->current_tk.ty].infix;
    node = inf(parser, node);
  }
  return node;
}

static AstNode* parse_num(Parser* parser) {
  int line = parser->current_tk.line;
  consume(parser, TK_NUM);
  Token tok = parser->previous_tk;
  char* endptr;
  double val;
  if (*tok.value == '0' && tolower(*(tok.value + 1)) == 'x') {
    val = (double)strtol(tok.value, &endptr, 16);
  } else {
    val = strtod(tok.value, &endptr);
  }
  ASSERT(tok.value + tok.length == endptr);
  AstNode* node = new_num(&parser->store, val, line);
  return node;
}

static AstNode* parse_binary(Parser* parser, AstNode* left) {
  int line = parser->current_tk.line;
  BindingPower bp = p_table[parser->current_tk.ty].bp;
  OpTy op = 0xff;
  switch (parser->current_tk.ty) {
    case TK_PLUS:
      op = OP_PLUS;
      break;
    case TK_MINUS:
      op = OP_MINUS;
      break;
    case TK_F_SLASH:
      op = OP_DIV;
      break;
    case TK_STAR:
      op = OP_MUL;
      break;
    default:
      parse_error(parser, parser->current_tk, E003, NULL);
  }
  advance(parser);
  AstNode* right = _parse(parser, bp);
  return new_binary(&parser->store, left, right, line, op);
}

AstNode* expr(Parser* parser) {
  return _parse(parser, BP_ASSIGNMENT);
}

AstNode* parse(Parser* parser) {
  advance(parser);
  AstNode* node = expr(parser);
  consume(parser, TK_EOF);
  return node;
}
