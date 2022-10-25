#include "parser.h"

typedef AstNode* (*PrefixFn)(Parser*);
typedef AstNode* (*InfixFn)(Parser*, AstNode*);

// clang-format off
typedef enum BindingPower {
  BP_NONE,        // other
  BP_ASSIGNMENT,  //
  BP_TERM,        // +, -
  BP_FACTOR,      // /, *, %
  BP_POWER,       // **
  BP_UNARY,       // !, -, +, ~
} BindingPower;
// clang-format on

typedef struct {
  BindingPower bp;
  PrefixFn prefix;
  InfixFn infix;
} ParseTable;

static AstNode* _parse(Parser* parser, BindingPower bp);
static AstNode* parse_num(Parser* parser);
static AstNode* parse_unary(Parser* parser);
static AstNode* parse_literal(Parser* parser);
static AstNode* parse_binary(Parser* parser, AstNode* left);

// clang-format off
ParseTable p_table[] = {
  [TK_NUM] = {.bp = BP_NONE, .prefix = parse_num, .infix = NULL},
  [TK_PLUS] = {.bp = BP_TERM, .prefix = parse_unary, .infix = parse_binary},
  [TK_MINUS] = {.bp = BP_TERM, .prefix = parse_unary, .infix = parse_binary},
  [TK_STAR] = {.bp = BP_FACTOR, .prefix = NULL, .infix = parse_binary},
  [TK_PERCENT] = {.bp = BP_FACTOR, .prefix = NULL, .infix = parse_binary},
  [TK_STAR_STAR] = {.bp = BP_POWER, .prefix = NULL, .infix = parse_binary},
  [TK_EXC_MARK] = {.bp = BP_UNARY, .prefix = parse_unary, .infix = NULL},
  [TK_TILDE] = {.bp = BP_UNARY, .prefix = parse_unary, .infix = NULL},
  [TK_F_SLASH] = {.bp = BP_FACTOR, .prefix = NULL, .infix = parse_binary},
  [TK_FALSE] = {.bp = BP_NONE, .prefix = parse_literal, .infix = NULL},
  [TK_TRUE] = {.bp = BP_NONE, .prefix = parse_literal, .infix = NULL},
  [TK_NONE] = {.bp = BP_NONE, .prefix = parse_literal, .infix = NULL},
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
  while (bp < p_table[parser->current_tk.ty].bp) {
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
  ASSERT(tok.value + tok.length == endptr, "failed to convert number");
  AstNode* node = new_num(&parser->store, val, line);
  return node;
}

static AstNode* parse_literal(Parser* parser) {
  int false_val = 0, true_val = 0, none_val = 0;
  int line = parser->current_tk.line;
  switch (parser->current_tk.ty) {
    case TK_FALSE:
      false_val = 1;
      break;
    case TK_TRUE:
      true_val = 1;
      break;
    case TK_NONE:
      none_val = 1;
      break;
    default:
      UNREACHABLE("parse-literal");
  }
  advance(parser);
  AstNode* node = new_node(&parser->store);
  node->unit = (UnitNode) {
      .type = AST_UNIT,
      .line = line,
      .is_bool = false_val || true_val,
      .is_none = none_val,
      .is_false_bool = false_val,
      .is_true_bool = true_val};
  return node;
}

static AstNode* parse_unary(Parser* parser) {
  OpTy op = get_op(parser->current_tk.ty);
  int line = parser->current_tk.line;
  BindingPower bp = p_table[parser->current_tk.ty].bp;
  advance(parser);
  AstNode* node = _parse(parser, bp);
  return new_unary(&parser->store, node, line, op);
}

static AstNode* parse_binary(Parser* parser, AstNode* left) {
  int line = parser->current_tk.line;
  BindingPower bp = p_table[parser->current_tk.ty].bp;
  // this is guaranteed to be valid by the ParseTable
  OpTy op = get_op(parser->current_tk.ty);
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
