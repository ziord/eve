#include "parser.h"

#define BUFFER_SIZE (BUFFER_INIT_SIZE << 4)
#define CREATE_BUFFER(_buff, ...) \
  char _buff[BUFFER_SIZE]; \
  snprintf(_buff, BUFFER_SIZE, __VA_ARGS__);

typedef AstNode* (*PrefixFn)(Parser*, bool);
typedef AstNode* (*InfixFn)(Parser*, AstNode*, bool);
extern Error error_types[];
extern char* token_types[];
extern AstNode* error_node;

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
} ExprParseTable;

static AstNode* _parse(Parser* parser, BindingPower bp);
static AstNode* parse_expr(Parser* parser);
static AstNode* parse_num(Parser* parser, bool assignable);
static AstNode* parse_unary(Parser* parser, bool assignable);
static AstNode* parse_literal(Parser* parser, bool assignable);
static AstNode*
parse_binary(Parser* parser, AstNode* left, bool assignable);
static AstNode* parse_string(Parser* parser, bool assignable);
static AstNode* parse_grouping(Parser* parser, bool assignable);
static AstNode* parse_list(Parser* parser, bool assignable);
static AstNode* parse_map(Parser* parser, bool assignable);
static AstNode*
parse_subscript(Parser* parser, AstNode* left, bool assignable);
static AstNode* parse_var(Parser* parser, bool assignable);

// clang-format off
ExprParseTable p_table[] = {
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
  [TK_COMMA] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_LBRACK] = {.bp = BP_NONE, .prefix = parse_grouping, .infix = NULL},
  [TK_RBRACK] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_LSQ_BRACK] = {.bp = BP_ACCESS, .prefix = parse_list, .infix = parse_subscript},
  [TK_RSQ_BRACK] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_HASH] = {.bp = BP_NONE, .prefix = parse_map, .infix = NULL},
  [TK_COLON] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_SEMI_COLON] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_LCURLY] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_RCURLY] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_CARET] = {.bp = BP_XOR, .prefix = NULL, .infix = parse_binary},
  [TK_AMP] = {.bp = BP_BW_AND, .prefix = NULL, .infix = parse_binary},
  [TK_LSHIFT] = {.bp = BP_SHIFT, .prefix = NULL, .infix = parse_binary},
  [TK_RSHIFT] = {.bp = BP_SHIFT, .prefix = NULL, .infix = parse_binary},
  [TK_FALSE] = {.bp = BP_NONE, .prefix = parse_literal, .infix = NULL},
  [TK_TRUE] = {.bp = BP_NONE, .prefix = parse_literal, .infix = NULL},
  [TK_NONE] = {.bp = BP_NONE, .prefix = parse_literal, .infix = NULL},
  [TK_SHOW] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_LET] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_IDENT] = {.bp = BP_NONE, .prefix = parse_var, .infix = NULL},
  [TK_STRING] = {.bp = BP_NONE, .prefix = parse_string, .infix = NULL},
  [TK_EOF] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_ERROR] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
};
// clang-format on

typedef struct {
  char* err_msg;
  char* hlp_msg;
  bool is_warning;
  Token* token;
} ErrorArgs;

static void advance(Parser* parser);

Parser new_parser(char* src, const char* fp) {
  Lexer lexer;
  init_lexer(&lexer, src);
  Parser parser =
      {.lexer = lexer, .file_path = fp, .errors = 0, .panicking = false};
  init_store(&parser.store);
  return parser;
}

void free_parser(Parser* parser) {
  free_store(&parser->store);
}

inline static AstNode* new_node(Parser* parser) {
  return new_ast_node(&parser->store);
}

ErrorArgs new_error_arg(Token* token, char* err, char* hlp) {
  return (ErrorArgs) {
      .token = token,
      .err_msg = err,
      .hlp_msg = hlp,
      .is_warning = false};
}

void display_error(Parser* parser, ErrorTy ty, ErrorArgs* args) {
  /*
   * optional_args:
   *  - is_warning
   *  - token
   *  - help_msg
   *  - err_msg
   */
  bool is_warning = args ? args->is_warning : false;
  Error error = error_types[ty];
  char* warning_msg = is_warning ? "[Warning] " : " ";
  Token token = args ? (args->token ? *args->token : parser->current_tk)
                     : parser->current_tk;
  char* help_msg = args ? (args->hlp_msg ? args->hlp_msg : error.hlp_msg)
                        : error.hlp_msg;
  char* err_msg = args ? (args->err_msg ? args->err_msg : error.err_msg)
                       : error.err_msg;
  parser->file_path ? fprintf(stderr, "~ %s ~\n", parser->file_path)
                    : (void)0;
  fprintf(
      stderr,
      "%4d |[E%04d]%s%s\n",
      token.line,
      ty,
      warning_msg,
      err_msg);
  int squi_padding, src_len;
  char* src =
      get_src_at_line(&parser->lexer, token, &squi_padding, &src_len);
  squi_padding = abs(squi_padding);
  int squi_len = token.length <= 0 ? 1 : token.length;
  fprintf(stderr, "%6s %.*s\n", "|", src_len, src);
  fprintf(stderr, "%6s ", "|");
  for (int i = 0; i < squi_padding; i++)
    fputc(' ', stderr);
  for (int i = 0; i < squi_len; i++)
    fputc('^', stderr);
  fputs("\n", stderr);
  if (help_msg) {
    fprintf(stderr, "%6s Help:\n", "|");
    fprintf(stderr, "%6s %s\n", "|", help_msg);
  }
}

AstNode* parse_error(Parser* parser, ErrorTy ty, ErrorArgs* args) {
  //  parser->at_error = true;
  if (!parser->panicking) {
    parser->errors++;
    parser->panicking = true;
    if (parser->errors > 1) {
      fputc('\n', stderr);
    }
    display_error(parser, ty, args);
  }
  return error_node;
}

void advance(Parser* parser) {
  parser->previous_tk = parser->current_tk;
  Token tok = get_token(&parser->lexer);
  if (tok.ty == TK_ERROR) {
    ErrorArgs args = new_error_arg(&tok, NULL, NULL);
    parse_error(parser, tok.error_ty, &args);
  } else {
    parser->current_tk = tok;
  }
}

void consume(Parser* parser, TokenTy ty) {
  if (parser->current_tk.ty == ty) {
    advance(parser);
  } else {
    CREATE_BUFFER(
        buff,
        error_types[E0004].hlp_msg,
        token_types[ty],
        token_types[parser->current_tk.ty])
    ErrorArgs args = new_error_arg(NULL, NULL, buff);
    parse_error(parser, E0004, &args);
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

inline static int count_escapes(char* src, int len) {
  char* p = strchr(src, '\\');
  int count = 0;
  while (p != NULL) {
    if (p - src >= len) {
      break;
    }
    count++;
    p = strchr(p + 1, '\\');
  }
  return count;
}

inline static bool is_assignable_op(TokenTy ty) {
  switch (ty) {
    case TK_PLUS:
    case TK_MINUS:
    case TK_STAR:
    case TK_STAR_STAR:
    case TK_FSLASH:
    case TK_PERCENT:
    case TK_AMP:
    case TK_CARET:
    case TK_PIPE:
    case TK_LBRACK:
    case TK_LSHIFT:
    case TK_RSHIFT:
      return true;
    default:
      return false;
  }
}

static AstNode* _parse(Parser* parser, BindingPower bp) {
  PrefixFn pref = p_table[parser->current_tk.ty].prefix;
  if (pref == NULL) {
    CREATE_BUFFER(
        buff,
        error_types[E0005].err_msg,
        token_types[parser->current_tk.ty])
    ErrorArgs args = new_error_arg(NULL, buff, NULL);
    return parse_error(parser, E0005, &args);
  }
  bool assignable = bp <= BP_ASSIGNMENT;
  AstNode* node = pref(parser, assignable);
  while (bp < p_table[parser->current_tk.ty].bp) {
    InfixFn inf = p_table[parser->current_tk.ty].infix;
    node = inf(parser, node, assignable);
  }
  return node;
}

AstNode* handle_aug_assign(
    Parser* parser,
    AstNode* node,
    bool assignable,
    Token tok) {
  //* handle augmented assignments *//
  if (assignable) {
    if (is_assignable_op(parser->current_tk.ty)) {
      if (is_current_symbol(&parser->lexer, '=')) {
        // +=, -=, ...=
        // e.g. x += expr => x = x + expr;
        tok = parser->current_tk;
        OpTy op = get_op(parser->current_tk.ty);
        advance(parser);  // skip assignable op
        advance(parser);  // skip '='
        AstNode* value = parse_expr(parser);
        AstNode* binop = new_node(parser);
        binop->binary = (BinaryNode) {
            .line = tok.line,
            .type = AST_BINARY,
            .l_node = node,
            .r_node = value,
            .op = op};
        AstNode* assignment = new_node(parser);
        assignment->binary = (BinaryNode) {
            .line = tok.line,
            .type = AST_ASSIGN,
            .l_node = node,
            .r_node = binop,
            .op = get_op(TK_EQ)};
        return assignment;
      }
    } else if (match(parser, TK_EQ)) {
      AstNode* value = parse_expr(parser);
      AstNode* assignment = new_node(parser);
      assignment->binary = (BinaryNode) {
          .line = tok.line,
          .type = AST_ASSIGN,
          .l_node = node,
          .r_node = value,
          .op = get_op(TK_EQ)};
      return assignment;
    }
  }
  return node;
}

static AstNode* parse_num(Parser* parser, bool assignable) {
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

static AstNode* parse_string(Parser* parser, bool assignable) {
  AstNode* node = new_node(parser);
  Token tok = parser->current_tk;
  char* src = tok.value + 1;  // skip opening quot
  int len = tok.length - 2;  // exclude opening & closing quot
  char* str;
  if (tok.has_esc) {
    int real_len = len - count_escapes(src, len);
    str = alloc(NULL, real_len + 1);
    ASSERT(
        copy_str(src, &str, len) == real_len,
        "invalid escape computation");
    str[real_len] = '\0';
    // update len and copy
    len = real_len;
  } else {
    str = src;
  }
  node->str = (StringNode) {
      .type = AST_STR,
      .start = str,
      .length = len,
      .is_alloc = tok.has_esc,
      .line = parser->current_tk.line};
  advance(parser);  // skip the string token
  return node;
}

static AstNode* parse_grouping(Parser* parser, bool assignable) {
  advance(parser);  // skip '(' token
  AstNode* node = parse_expr(parser);
  consume(parser, TK_RBRACK);
  return node;
}

static AstNode* parse_list(Parser* parser, bool assignable) {
  advance(parser);  // skip '[' token
  AstNode* node = new_node(parser);
  ListNode* list = &node->list;
  list->type = AST_LIST;
  list->line = parser->previous_tk.line;
  list->len = 0;
  while (!match(parser, TK_RSQ_BRACK)) {
    if (list->len > 0) {
      consume(parser, TK_COMMA);
    }
    if (list->len > BYTE_MAX) {
      CREATE_BUFFER(buff, error_types[E0007].hlp_msg, BYTE_MAX)
      ErrorArgs args = new_error_arg(NULL, NULL, buff);
      return parse_error(parser, E0007, &args);
    }
    list->elems[list->len++] = parse_expr(parser);
  }
  return node;
}

static AstNode* parse_map(Parser* parser, bool assignable) {
  // #{key: value}
  int line = parser->current_tk.line;
  advance(parser);  // skip '#' token
  consume(parser, TK_LCURLY);
  AstNode* node = new_node(parser);
  MapNode* map = &node->map;
  map->line = line;
  map->length = 0;
  map->type = AST_MAP;
  AstNode *value, *key;
  while (!match(parser, TK_RCURLY)) {
    if (map->length > BYTE_MAX) {
      CREATE_BUFFER(buff, error_types[E0009].hlp_msg, BYTE_MAX)
      ErrorArgs args = new_error_arg(NULL, NULL, buff);
      return parse_error(parser, E0009, &args);
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

static AstNode* parse_literal(Parser* parser, bool assignable) {
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
  AstNode* node = new_node(parser);
  node->unit = (UnitNode) {
      .type = AST_UNIT,
      .line = line,
      .is_bool = false_val || true_val,
      .is_none = none_val,
      .is_false_bool = false_val,
      .is_true_bool = true_val};
  return node;
}

static AstNode* parse_unary(Parser* parser, bool assignable) {
  OpTy op = get_op(parser->current_tk.ty);
  int line = parser->current_tk.line;
  BindingPower bp = p_table[parser->current_tk.ty].bp;
  advance(parser);
  AstNode* node = _parse(parser, bp);
  return new_unary(&parser->store, node, line, op);
}

static AstNode*
parse_binary(Parser* parser, AstNode* left, bool assignable) {
  int line = parser->current_tk.line;
  BindingPower bp = p_table[parser->current_tk.ty].bp;
  // this is guaranteed to be valid by the ExprParseTable
  OpTy op = get_op(parser->current_tk.ty);
  advance(parser);
  AstNode* right = _parse(parser, bp);
  return new_binary(&parser->store, left, right, line, op);
}

static AstNode*
parse_subscript(Parser* parser, AstNode* left, bool assignable) {
  Token tok = parser->current_tk;
  advance(parser);  // skip '[' token
  AstNode* expr = parse_expr(parser);
  consume(parser, TK_RSQ_BRACK);
  AstNode* node = new_node(parser);
  SubscriptNode* subscript = &node->subscript;
  subscript->type = AST_SUBSCRIPT;
  subscript->line = tok.line;
  subscript->expr = left;
  subscript->subscript = expr;
  return handle_aug_assign(parser, node, assignable, tok);
}

static AstNode* parse_expr(Parser* parser) {
  return _parse(parser, BP_ASSIGNMENT);
}

static AstNode* parse_expr_stmt(Parser* parser) {
  int line = parser->current_tk.line;
  AstNode* exp = parse_expr(parser);
  consume(parser, TK_SEMI_COLON);
  AstNode* stmt = new_node(parser);
  ExprStmtNode* expr_stmt = &stmt->expr_stmt;
  expr_stmt->type = AST_EXPR_STMT;
  expr_stmt->line = line;
  expr_stmt->expr = exp;
  return stmt;
}

static AstNode* parse_show_stmt(Parser* parser) {
  advance(parser);  // skip token 'show'
  AstNode* stmt = new_node(parser);
  ShowStmtNode* show_stmt = &stmt->show_stmt;
  show_stmt->line = parser->previous_tk.line;
  show_stmt->type = AST_SHOW_STMT;
  show_stmt->length = 0;
  do {
    if (show_stmt->length > BYTE_MAX) {
      CREATE_BUFFER(buff, error_types[E0010].hlp_msg, BYTE_MAX)
      ErrorArgs args = new_error_arg(NULL, NULL, buff);
      return parse_error(parser, E0010, &args);
    }
    if (show_stmt->length > 0) {
      consume(parser, TK_COMMA);
    }
    show_stmt->items[show_stmt->length++] = parse_expr(parser);
  } while (!match(parser, TK_SEMI_COLON));
  return stmt;
}

static AstNode* parse_assert_stmt(Parser* parser) {
  Token tok = parser->current_tk;
  advance(parser);  // skip token 'assert'
  AstNode* stmt = new_node(parser);
  AstNode* test = parse_expr(parser);
  AstNode* msg = NULL;
  if (match(parser, TK_COMMA)) {
    msg = parse_expr(parser);
  } else {
    char* buff = "Assertion failed. Test is falsy.";
    msg = new_node(parser);
    msg->str = (StringNode) {
        .type = AST_STR,
        .line = parser->previous_tk.line,
        .is_alloc = false,
        .start = buff,
        .length = (int)strlen(buff)};
  }
  stmt->assert_stmt = (AssertStmtNode) {
      .type = AST_ASSERT_STMT,
      .line = tok.line,
      .test = test,
      .msg = msg};
  consume(parser, TK_SEMI_COLON);
  return stmt;
}

static AstNode* parse_stmt(Parser* parser) {
  if (is_tty(parser, TK_SHOW)) {
    return parse_show_stmt(parser);
  } else if (is_tty(parser, TK_ASSERT)) {
    return parse_assert_stmt(parser);
  }
  return parse_expr_stmt(parser);
}

static AstNode* parse_var_decl(Parser* parser) {
  // let foo = expr;
  int line = parser->current_tk.line;
  AstNode* var = new_node(parser);
  advance(parser);
  consume(parser, TK_IDENT);
  var->var = (VarNode) {
      .type = AST_VAR,
      .name = parser->previous_tk.value,
      .len = parser->previous_tk.length,
      .line = line};
  consume(parser, TK_EQ);
  AstNode* value = parse_expr(parser);
  AstNode* decl = new_node(parser);
  decl->binary = (BinaryNode) {
      .type = AST_VAR_DECL,
      .line = line,
      .l_node = var,
      .r_node = value,
      .op = get_op(TK_EQ)};
  consume(parser, TK_SEMI_COLON);
  return decl;
}

static AstNode* parse_var(Parser* parser, bool assignable) {
  advance(parser);
  Token tok = parser->previous_tk;
  AstNode* node = new_node(parser);
  node->var = (VarNode) {
      .line = tok.line,
      .len = tok.length,
      .name = tok.value,
      .type = AST_VAR};
  return handle_aug_assign(parser, node, assignable, tok);
}

void resync(Parser* parser) {
  parser->panicking = false;
  for (;;) {
    switch (parser->current_tk.ty) {
      case TK_LET:
      case TK_SHOW:
      case TK_ASSERT:
      case TK_EOF:
        return;
      default:
        advance(parser);
    }
  }
}

static AstNode* parse_decls(Parser* parser) {
  if (is_tty(parser, TK_LET)) {
    return parse_var_decl(parser);
  }
  return parse_stmt(parser);
}

static AstNode* parse_program(Parser* parser) {
  AstNode* node = new_node(parser);
  node->program.type = AST_PROGRAM;
  vec_init(&node->program.decls);
  while (!match(parser, TK_EOF)) {
    if (parser->panicking) {
      resync(parser);
      // just in case we hit eof after re-syncing,
      // we should immediately end parsing
      if (is_tty(parser, TK_EOF)) {
        break;
      }
    }
    vec_push(&node->program.decls, parse_decls(parser));
  }
  return node;
}

AstNode* parse(Parser* parser) {
  advance(parser);
  AstNode* node = parse_program(parser);
  consume(parser, TK_EOF);
  return node;
}

#undef CREATE_BUFFER
