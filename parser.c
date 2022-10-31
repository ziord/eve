#include "parser.h"

typedef AstNode* (*PrefixFn)(Parser*);
typedef AstNode* (*InfixFn)(Parser*, AstNode*);

// clang-format off
typedef enum BindingPower {
  BP_NONE,        // other
  BP_ASSIGNMENT,  //
  BP_OR,          // ||
  BP_AND,         // &&
  BP_BW_OR,       // |
  BP_XOR,         // ^
  BP_BW_AND,      // &
  BP_EQUALITY,    // !=, ==
  BP_COMPARISON,  // >, >=, <, <=
  BP_SHIFT,       // >>, <<
  BP_TERM,        // +, -
  BP_FACTOR,      // /, *, %
  BP_POWER,       // **
  BP_UNARY,       // !, -, +, ~
  BP_ACCESS,      // [], .
} BindingPower;
// clang-format on

typedef struct {
  BindingPower bp;
  PrefixFn prefix;
  InfixFn infix;
} ParseTable;

static AstNode* _parse(Parser* parser, BindingPower bp);
static AstNode* parse_expr(Parser* parser);
static AstNode* parse_num(Parser* parser);
static AstNode* parse_unary(Parser* parser);
static AstNode* parse_literal(Parser* parser);
static AstNode* parse_binary(Parser* parser, AstNode* left);
static AstNode* parse_string(Parser* parser);
static AstNode* parse_grouping(Parser* parser);
static AstNode* parse_list(Parser* parser);
static AstNode* parse_map(Parser* parser);
static AstNode* parse_subscript(Parser* parser, AstNode* left);

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
  [TK_FSLASH] = {.bp = BP_FACTOR, .prefix = NULL, .infix = parse_binary},
  [TK_GRT] = {.bp = BP_COMPARISON, .prefix = NULL, .infix = parse_binary},
  [TK_LESS] = {.bp = BP_COMPARISON, .prefix = NULL, .infix = parse_binary},
  [TK_GRT_EQ] = {.bp = BP_COMPARISON, .prefix = NULL, .infix = parse_binary},
  [TK_LESS_EQ] = {.bp = BP_COMPARISON, .prefix = NULL, .infix = parse_binary},
  [TK_NOT_EQ] = {.bp = BP_EQUALITY, .prefix = NULL, .infix = parse_binary},
  [TK_EQ_EQ] = {.bp = BP_EQUALITY, .prefix = NULL, .infix = parse_binary},
  [TK_EQ] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_PIPE_PIPE] = {.bp = BP_OR, .prefix = NULL, .infix = parse_binary},
  [TK_AMP_AMP] = {.bp = BP_AND, .prefix = NULL, .infix = parse_binary},
  [TK_PIPE] = {.bp = BP_BW_OR, .prefix = NULL, .infix = parse_binary},
  [TK_LBRACK] = {.bp = BP_NONE, .prefix = parse_grouping, .infix = NULL},
  [TK_RBRACK] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_LSQ_BRACK] = {.bp = BP_ACCESS, .prefix = parse_list, .infix = parse_subscript},
  [TK_RSQ_BRACK] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_HASH] = {.bp = BP_NONE, .prefix = parse_map, .infix = NULL},
  [TK_COLON] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_LCURLY] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_RCURLY] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_COLON] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_CARET] = {.bp = BP_XOR, .prefix = NULL, .infix = parse_binary},
  [TK_AMP] = {.bp = BP_BW_AND, .prefix = NULL, .infix = parse_binary},
  [TK_LSHIFT] = {.bp = BP_SHIFT, .prefix = NULL, .infix = parse_binary},
  [TK_RSHIFT] = {.bp = BP_SHIFT, .prefix = NULL, .infix = parse_binary},
  [TK_FALSE] = {.bp = BP_NONE, .prefix = parse_literal, .infix = NULL},
  [TK_TRUE] = {.bp = BP_NONE, .prefix = parse_literal, .infix = NULL},
  [TK_NONE] = {.bp = BP_NONE, .prefix = parse_literal, .infix = NULL},
  [TK_STRING] = {.bp = BP_NONE, .prefix = parse_string, .infix = NULL},
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

static AstNode* parse_string(Parser* parser) {
  AstNode* node = new_node(&parser->store);
  node->str = (StringNode) {
      .type = AST_STR,
      // skip opening quot
      .start = ++parser->current_tk.value,
      // exclude opening & closing quot
      .length = parser->current_tk.length - 2,
      .line = parser->current_tk.line};
  advance(parser);  // skip the string token
  return node;
}

static AstNode* parse_grouping(Parser* parser) {
  advance(parser);  // skip '(' token
  AstNode* node = parse_expr(parser);
  consume(parser, TK_RBRACK);
  return node;
}

static AstNode* parse_list(Parser* parser) {
  advance(parser);  // skip '[' token
  AstNode* node = new_node(&parser->store);
  ListNode* list = &node->list;
  list->type = AST_LIST;
  list->line = parser->previous_tk.line;
  list->len = 0;
  while (!match(parser, TK_RSQ_BRACK)) {
    if (list->len > 0) {
      consume(parser, TK_COMMA);
    }
    if (list->len > BYTE_MAX) {
      parse_error(parser, parser->current_tk, E004, NULL);
    }
    list->elems[list->len++] = parse_expr(parser);
  }
  return node;
}

static AstNode* parse_map(Parser* parser) {
  // #{key: value}
  int line = parser->current_tk.line;
  advance(parser);  // skip '#' token
  consume(parser, TK_LCURLY);
  AstNode* node = new_node(&parser->store);
  MapNode* map = &node->map;
  map->line = line;
  map->length = 0;
  map->type = AST_MAP;
  AstNode *value, *key;
  while (!match(parser, TK_RCURLY)) {
    if (map->length > BYTE_MAX) {
      parse_error(parser, parser->current_tk, E006, NULL);
    }
    if (map->length > 0) {
      consume(parser, TK_COMMA);
    }
    key = parse_expr(parser);
    consume(parser, TK_COLON);
    value = parse_expr(parser);
    map->items[map->length][0] = key;
    map->items[map->length++][1] = value;
  }
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
  advance(parser);  // skip literal token
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

static AstNode* parse_subscript(Parser* parser, AstNode* left) {
  int line = parser->current_tk.line;
  advance(parser);  // skip '[' token
  AstNode* expr = parse_expr(parser);
  consume(parser, TK_RSQ_BRACK);
  AstNode* node = new_node(&parser->store);
  SubscriptNode* subscript = &node->subscript;
  subscript->type = AST_SUBSCRIPT;
  subscript->line = line;
  subscript->expr = left;
  subscript->subscript = expr;
  return node;
}

static AstNode* parse_expr(Parser* parser) {
  return _parse(parser, BP_ASSIGNMENT);
}

static AstNode* parse_expr_stmt(Parser* parser) {
  int line = parser->current_tk.line;
  AstNode* exp = parse_expr(parser);
  consume(parser, TK_SEMI_COLON);
  AstNode* stmt = new_node(&parser->store);
  ExprStmtNode* expr_stmt = &stmt->expr_stmt;
  expr_stmt->type = AST_EXPR_STMT;
  expr_stmt->line = line;
  expr_stmt->expr = exp;
  return stmt;
}

static AstNode* parse_show_stmt(Parser* parser) {
  advance(parser);  // skip token 'show'
  AstNode* stmt = new_node(&parser->store);
  ShowStmtNode* show_stmt = &stmt->show_stmt;
  show_stmt->line = parser->previous_tk.line;
  show_stmt->type = AST_SHOW_STMT;
  show_stmt->length = 0;
  do {
    if (show_stmt->length > BYTE_MAX) {
      parse_error(parser, parser->current_tk, E007, NULL);
    }
    if (show_stmt->length > 0) {
      consume(parser, TK_COMMA);
    }
    show_stmt->items[show_stmt->length++] = parse_expr(parser);
  } while (!match(parser, TK_SEMI_COLON));
  return stmt;
}

static AstNode* parse_stmt(Parser* parser) {
  if (is_tty(parser, TK_SHOW)) {
    return parse_show_stmt(parser);
  }
  return parse_expr_stmt(parser);
}

AstNode* parse(Parser* parser) {
  advance(parser);
  AstNode* node = parse_stmt(parser);
  consume(parser, TK_EOF);
  return node;
}
