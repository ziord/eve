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
  BP_CALL,        // ()
  BP_ACCESS,      // [], ., ::
} BindingPower;
// clang-format on

typedef struct {
  BindingPower bp;
  PrefixFn prefix;
  InfixFn infix;
} ExprParseTable;

static AstNode* _parse(Parser* parser, BindingPower bp);
static AstNode* parse_func_decl(Parser* parser, bool is_lambda);
static AstNode* parse_block_stmt(Parser* parser);
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
static AstNode* parse_try(Parser* parser, bool assignable);
static AstNode* parse_decls(Parser* parser);
static AstNode* parse_stmt(Parser* parser);
static AstNode* parse_func_expr(Parser* parser, bool assignable);
static AstNode* parse_call(Parser* parser, AstNode* left, bool assignable);
static AstNode*
parse_dcol_expr(Parser* parser, AstNode* left, bool assignable);
static AstNode*
parse_dot_expr(Parser* parser, AstNode* left, bool assignable);
static AstNode* parse_expr_stmt(Parser* parser);

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
  [TK_LBRACK] = {.bp = BP_CALL, .prefix = parse_grouping, .infix = parse_call},
  [TK_RBRACK] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_LSQ_BRACK] = {.bp = BP_ACCESS, .prefix = parse_list, .infix = parse_subscript},
  [TK_RSQ_BRACK] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_HASH] = {.bp = BP_NONE, .prefix = parse_map, .infix = NULL},
  [TK_COLON] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_DCOLON] = {.bp = BP_ACCESS, .prefix = NULL, .infix = parse_dcol_expr},
  [TK_SEMI_COLON] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_LCURLY] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_RCURLY] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_CARET] = {.bp = BP_XOR, .prefix = NULL, .infix = parse_binary},
  [TK_AMP] = {.bp = BP_BW_AND, .prefix = NULL, .infix = parse_binary},
  [TK_LSHIFT] = {.bp = BP_SHIFT, .prefix = NULL, .infix = parse_binary},
  [TK_RSHIFT] = {.bp = BP_SHIFT, .prefix = NULL, .infix = parse_binary},
  [TK_ARROW] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_AT] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_DOT] = {.bp = BP_ACCESS, .prefix = NULL, .infix = parse_dot_expr},
  [TK_QMARK] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_FALSE] = {.bp = BP_NONE, .prefix = parse_literal, .infix = NULL},
  [TK_TRUE] = {.bp = BP_NONE, .prefix = parse_literal, .infix = NULL},
  [TK_NONE] = {.bp = BP_NONE, .prefix = parse_literal, .infix = NULL},
  [TK_SHOW] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_LET] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_TRY] = {.bp = BP_ASSIGNMENT, .prefix = parse_try, .infix = NULL},
  [TK_THROW] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_IF] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_ELSE] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_ASSERT] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_FN] = {.bp = BP_NONE, .prefix = parse_func_expr, .infix = NULL},
  [TK_FOR] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_IN] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_WHILE] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_CONTINUE] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_RETURN] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_STRUCT] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
  [TK_BREAK] = {.bp = BP_NONE, .prefix = NULL, .infix = NULL},
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
  Parser parser = {
      .lexer = lexer,
      .file_path = fp,
      .errors = 0,
      .panicking = false,
      .loop = 0,
      .func = 0};
  init_store(&parser.store);
  return parser;
}

void free_parser(Parser* parser) {
  free_store(&parser->store);
}

inline static AstNode* new_node(Parser* parser) {
  return new_ast_node(&parser->store);
}

inline static ErrorArgs new_error_arg(Token* token, char* err, char* hlp) {
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
  fprintf(stderr, "%4s [E%04d]%s%s\n", " ", ty, warning_msg, err_msg);
  int squi_padding, src_len,
      squi_len = token.length <= 0 ? 1 : token.length;
  char* src =
      get_src_at_line(&parser->lexer, token, &squi_padding, &src_len);
  squi_padding = abs(squi_padding);
  if (squi_padding > src_len) {
    squi_padding = src_len;
  }
  if (squi_len > src_len) {
    squi_len = 1;
  }
  fprintf(stderr, "%4d %s %.*s\n", token.line, "|", src_len, src);
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
    if (args && !args->is_warning) {
      parser->errors++;
      parser->panicking = true;
    }
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

static AstNode* set_return(Parser* parser, BlockStmtNode* node) {
  AstNode *last = NULL, *ret = NULL;
  int line;
  if (node->stmts.len) {
    last = node->stmts.items[node->stmts.len - 1];
  } else {
    goto set_ret;
  }
  if (last->num.type != AST_RETURN_STMT) {
  set_ret:
    // inject "return None"
    ret = new_node(parser);
    line = parser->previous_tk.line;
    ret->expr_stmt = (ExprStmtNode) {
        .type = AST_RETURN_STMT,
        .line = line,
        .expr = new_none(&parser->store, line)};
    vec_push(&node->stmts, ret);
  } else {
    ret = last;
  }
  return ret;
}

inline static void can_tco(FuncNode* func, AstNode* ret_node) {
  // check if a function can be tail-call-optimized
  // this is a very limited/restricted tco check
  if (!func->name)
    return;
  ExprStmtNode* node = &ret_node->expr_stmt;
  if (node->expr->num.type != AST_CALL)
    return;
  CallNode* call = &node->expr->call;
  if (call->left->num.type != AST_VAR)
    return;
  VarNode* fn_name = &call->left->var;
  if (fn_name->len != func->name->var.len
      || memcmp(fn_name->name, func->name->var.name, fn_name->len) != 0) {
    return;
  }
  // TODO: There might have to be a restriction on the number of
  //       arguments the tco function is allowed to have
  call->is_tail_call = true;
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

static AstNode* parse_var(Parser* parser, bool assignable) {
  /*
   * ID ("::" expr)? | ID { (ID = expr ("," ID = expr)*)? }
   */
  Token prev_tok = parser->previous_tk;
  consume(parser, TK_IDENT);
  Token tok = parser->previous_tk;
  AstNode* node = new_node(parser);
  node->var = (VarNode) {
      .line = tok.line,
      .len = tok.length,
      .name = tok.value,
      .type = AST_VAR};
  bool is_struct = prev_tok.ty == TK_STRUCT;
  bool is_if = !is_struct && prev_tok.ty == TK_IF;
  bool is_while = !is_if && prev_tok.ty == TK_WHILE;
  bool is_in = !is_while && prev_tok.ty == TK_IN;
  bool is_allowable = !is_struct && !is_if && !is_while && !is_in;
  // ID { (ID = expr ("," ID = expr)*)? }
  if (is_allowable && assignable && is_tty(parser, TK_LCURLY)) {
    // we'll represent struct instances as a StructCallNode ast node,
    // with AST_STRUCT_CALL type, and its (key=value) fields stored in a map
    advance(parser);
    AstNode* st_call = new_node(parser);
    st_call->struct_call = (StructCallNode) {
        .type = AST_STRUCT_CALL,
        .name = node,
        .line = tok.line};
    MapNode* map = &st_call->struct_call.fields;
    *map = (MapNode) {.type = AST_MAP, .length = 0, .line = tok.line};
    while (!is_tty(parser, TK_RCURLY) && !is_tty(parser, TK_EOF)) {
      if (map->length > 0) {
        consume(parser, TK_COMMA);
      }
      consume(parser, TK_IDENT);
      AstNode* var = new_var(&parser->store, parser->previous_tk);
      consume(parser, TK_EQ);
      map->items[map->length][0] = var;
      map->items[map->length++][1] = parse_expr(parser);
      if (map->length > CONST_MAX) {
        CREATE_BUFFER(buff, error_types[E0015].hlp_msg, CONST_MAX)
        ErrorArgs args = new_error_arg(&parser->previous_tk, NULL, buff);
        return parse_error(parser, E0015, &args);
      }
    }
    consume(parser, TK_RCURLY);
    return st_call;
  }
  return handle_aug_assign(parser, node, assignable, tok);
}

static AstNode* parse_try(Parser* parser, bool assignable) {
  // try expr (? var)? (else expr)?
  int line = parser->current_tk.line;
  consume(parser, TK_TRY);
  AstNode* try_expr = parse_expr(parser);
  AstNode *else_expr = NULL, *var = NULL;
  if (match(parser, TK_QMARK)) {
    var = parse_var(parser, false);
  }
  if (match(parser, TK_ELSE)) {
    else_expr = parse_expr(parser);
  }
  AstNode* node = new_node(parser);
  node->try_h = (TryNode) {
      .type = AST_TRY,
      .try_expr = try_expr,
      .try_var = var,
      .else_expr = else_expr,
      .line = line};
  return node;
}

static AstNode* parse_throw(Parser* parser) {
  // throw expr;
  consume(parser, TK_THROW);
  AstNode* node = parse_expr_stmt(parser);
  node->num.type = AST_THROW;
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
  Token tok;
  while (!match(parser, TK_RSQ_BRACK)) {
    if (list->len > 0) {
      consume(parser, TK_COMMA);
    }
    tok = parser->current_tk;
    list->elems[list->len++] = parse_expr(parser);
    if (list->len > CONST_MAX) {
      CREATE_BUFFER(buff, error_types[E0007].hlp_msg, CONST_MAX)
      ErrorArgs args = new_error_arg(&tok, NULL, buff);
      return parse_error(parser, E0007, &args);
    }
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
  Token tok;
  while (!match(parser, TK_RCURLY)) {
    if (map->length > 0) {
      consume(parser, TK_COMMA);
    }
    tok = parser->current_tk;
    key = parse_expr(parser);
    consume(parser, TK_COLON);
    value = parse_expr(parser);
    map->items[map->length][0] = key;
    map->items[map->length++][1] = value;
    if (map->length > CONST_MAX) {
      CREATE_BUFFER(buff, error_types[E0009].hlp_msg, CONST_MAX)
      ErrorArgs args = new_error_arg(&tok, NULL, buff);
      return parse_error(parser, E0009, &args);
    }
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

static AstNode* parse_func_expr(Parser* parser, bool assignable) {
  /*
   * fn (params..) {
   *  stmt*
   *  }
   */
  return parse_func_decl(parser, true);
}

static AstNode* parse_call(Parser* parser, AstNode* left, bool assignable) {
  consume(parser, TK_LBRACK);
  AstNode* node = new_node(parser);
  node->call = (CallNode) {
      .type = AST_CALL,
      .line = parser->previous_tk.line,
      .left = left,
      .is_tail_call = false,
      .args_count = 0};
  Token tok;
  while (!is_tty(parser, TK_RBRACK) && !is_tty(parser, TK_EOF)) {
    if (node->call.args_count > 0) {
      consume(parser, TK_COMMA);
    }
    tok = parser->current_tk;
    node->call.args[node->call.args_count++] = parse_expr(parser);
    if (node->call.args_count > CONST_MAX) {
      CREATE_BUFFER(buff, error_types[E0010].hlp_msg, CONST_MAX)
      ErrorArgs args = new_error_arg(&tok, NULL, buff);
      return parse_error(parser, E0010, &args);
    }
  }
  consume(parser, TK_RBRACK);
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
parse_dcol_expr(Parser* parser, AstNode* left, bool assignable) {
  int line = parser->current_tk.line;
  BindingPower bp = p_table[parser->current_tk.ty].bp;
  // this is guaranteed to be valid by the ExprParseTable
  OpTy op = get_op(parser->current_tk.ty);
  advance(parser);
  if (!is_tty(parser, TK_IDENT)) {
    CREATE_BUFFER(
        buff,
        error_types[E0004].hlp_msg,
        token_types[TK_IDENT],
        token_types[parser->current_tk.ty])
    ErrorArgs args = new_error_arg(NULL, NULL, buff);
    return parse_error(parser, E0004, &args);
  }
  AstNode* right = _parse(parser, bp);
  return new_binary(&parser->store, left, right, line, op);
}

static AstNode*
parse_dot_expr(Parser* parser, AstNode* left, bool assignable) {
  Token tok = parser->current_tk;
  AstNode* dot_expr = parse_dcol_expr(parser, left, assignable);
  dot_expr->num.type = AST_DOT_EXPR;
  return handle_aug_assign(parser, dot_expr, assignable, tok);
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
  consume(parser, TK_SHOW);
  AstNode* stmt = new_node(parser);
  ShowStmtNode* show_stmt = &stmt->show_stmt;
  show_stmt->line = parser->previous_tk.line;
  show_stmt->type = AST_SHOW_STMT;
  show_stmt->length = 0;
  Token tok;
  do {
    if (show_stmt->length > 0) {
      consume(parser, TK_COMMA);
    }
    tok = parser->current_tk;
    show_stmt->items[show_stmt->length++] = parse_expr(parser);
    if (show_stmt->length > CONST_MAX) {
      CREATE_BUFFER(buff, error_types[E0010].hlp_msg, CONST_MAX)
      ErrorArgs args = new_error_arg(&tok, NULL, buff);
      return parse_error(parser, E0010, &args);
    }
  } while (!match(parser, TK_SEMI_COLON));
  return stmt;
}

static AstNode* parse_assert_stmt(Parser* parser) {
  Token tok = parser->current_tk;
  consume(parser, TK_ASSERT);
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

static AstNode* parse_block_stmt(Parser* parser) {
  consume(parser, TK_LCURLY);
  AstNode* node = new_node(parser);
  node->block_stmt = (BlockStmtNode) {
      .type = AST_BLOCK_STMT,
      .line = parser->previous_tk.line};
  BlockStmtNode* block = &node->block_stmt;
  vec_init(&block->stmts);
  while (!is_tty(parser, TK_RCURLY) && !is_tty(parser, TK_EOF)) {
    vec_push(&block->stmts, parse_decls(parser));
    if (parser->panicking)
      break;
  }
  consume(parser, TK_RCURLY);
  return node;
}

static AstNode* parse_if_stmt(Parser* parser) {
  /*
   * if cond {
   *  stmt
   * } else {
   *  stmt
   * }
   */
  consume(parser, TK_IF);
  int line = parser->previous_tk.line;
  AstNode* cond = parse_expr(parser);
  AstNode* if_block = parse_block_stmt(parser);
  AstNode* else_block;
  if (match(parser, TK_ELSE)) {
    // only '{' or 'if' token can succeed an 'else' token
    if (is_tty(parser, TK_IF)) {
      else_block = parse_stmt(parser);
    } else {
      else_block = parse_block_stmt(parser);
    }
  } else {
    else_block = new_node(parser);
    else_block->block_stmt.type = AST_BLOCK_STMT;
    else_block->block_stmt.line = parser->previous_tk.line;
    vec_init(&else_block->block_stmt.stmts);
  }
  AstNode* node = new_node(parser);
  node->ife_stmt = (IfElseStmtNode) {
      .type = AST_IF_STMT,
      .condition = cond,
      .if_block = if_block,
      .else_block = else_block,
      .line = line};
  return node;
}

static AstNode* parse_control_stmt(Parser* parser) {
  if (!parser->loop) {
    CREATE_BUFFER(
        buff,
        error_types[E0006].err_msg,
        token_types[parser->current_tk.ty]);
    ErrorArgs args = new_error_arg(NULL, buff, NULL);
    return parse_error(parser, E0006, &args);
  }
  bool is_break = match(parser, TK_BREAK);
  bool is_continue = !is_break && match(parser, TK_CONTINUE);
  ASSERT(
      is_break || is_continue,
      "loop control should be 'break' or 'continue'");
  AstNode* node = new_node(parser);
  node->control_stmt = (ControlStmtNode) {
      .type = AST_CONTROL_STMT,
      .is_continue = is_continue,
      .is_break = is_break,
      .line = parser->previous_tk.line};
  consume(parser, TK_SEMI_COLON);
  return node;
}

static AstNode* parse_while_stmt(Parser* parser) {
  /*
   * while cond {
   *  stmt
   * }
   */
  parser->loop++;
  consume(parser, TK_WHILE);
  int line = parser->previous_tk.line;
  AstNode* cond = parse_expr(parser);
  AstNode* block = parse_block_stmt(parser);
  AstNode* node = new_node(parser);
  node->while_stmt = (WhileStmtNode) {
      .type = AST_WHILE_STMT,
      .condition = cond,
      .block = block,
      .line = line};
  parser->loop--;
  return node;
}

static AstNode* parse_for_stmt(Parser* parser) {
  /*
   * for let elem in iterable {
   *  stmt
   * }
   */
  parser->loop++;
  int line = parser->current_tk.line;
  int column = parser->lexer.column;
  consume(parser, TK_FOR);
  consume(parser, TK_LET);
  AstNode* elem = parse_var(parser, false);
  consume(parser, TK_IN);
  AstNode* iterable = parse_expr(parser);
  AstNode* block = parse_block_stmt(parser);
  AstNode* node = new_node(parser);
  node->for_stmt = (ForStmtNode) {
      .line = line,
      .type = AST_FOR_STMT,
      .block = block,
      .elem = elem,
      .iterable = iterable};

  char* template =
      "{\n"
      "        let is_error = false;\n"
      "        let itr = core::iter(obj);\n"
      "        while true {\n"
      "            let i = try core::next(itr) else is_error = true;\n"
      "            if is_error {\n"
      "                break;\n"
      "            }\n"
      "        }\n"
      "}";
  Parser prs = new_parser(template, "");
  prs.lexer.line = line;
  prs.lexer.column = column;
  AstNode* desugar = parse(&prs);
  node->for_stmt.desugar = desugar->program.decls.items[0];
  vec_free(&desugar->program.decls);  // free ProgramNode Vec decls
  // store all allocated nodes in the original parser.
  vec_extend(&parser->store.nodes, &prs.store.nodes);
  parser->loop--;
  return node;
}

static AstNode* parse_return_stmt(Parser* parser) {
  if (!parser->func) {
    AstNode* node = parse_error(parser, E0008, NULL);
    advance(parser);  // skip 'return'
    return node;
  }
  consume(parser, TK_RETURN);
  AstNode* node = new_node(parser);
  node->expr_stmt = (ExprStmtNode) {
      .type = AST_RETURN_STMT,
      .line = parser->previous_tk.line,
      .expr = NULL};
  if (!match(parser, TK_SEMI_COLON)) {
    node->expr_stmt.expr = parse_expr(parser);
    consume(parser, TK_SEMI_COLON);
  } else {
    node->expr_stmt.expr = new_none(&parser->store, node->expr_stmt.line);
  }
  return node;
}

static AstNode* parse_stmt(Parser* parser) {
  switch (parser->current_tk.ty) {
    case TK_SHOW:
      return parse_show_stmt(parser);
    case TK_ASSERT:
      return parse_assert_stmt(parser);
    case TK_LCURLY:
      return parse_block_stmt(parser);
    case TK_IF:
      return parse_if_stmt(parser);
    case TK_WHILE:
      return parse_while_stmt(parser);
    case TK_FOR:
      return parse_for_stmt(parser);
    case TK_BREAK:
    case TK_CONTINUE:
      return parse_control_stmt(parser);
    case TK_RETURN:
      return parse_return_stmt(parser);
    case TK_THROW:
      return parse_throw(parser);
    default:
      return parse_expr_stmt(parser);
  }
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

static AstNode* parse_func_decl(Parser* parser, bool is_lambda) {
  /*
   * fn my_func(param...) {
   *  stmt*
   *  }
   */

  Token prev_tok = parser->previous_tk;
  Token curr_tok = parser->current_tk;
  consume(parser, TK_FN);
  AstNode* name = NULL;
  // handle (top-level) function declarations
  if (!is_lambda) {
    // check so we don't mistake a function expression (lambda) as a function
    // declaration, this can happen when a lambda is a standalone expression
    // statement, i.e. fn () {...} ;
    if (!is_tty(parser, TK_IDENT)) {
      rewind_state(&parser->lexer, curr_tok);
      parser->current_tk = curr_tok;
      parser->previous_tk = prev_tok;
      return parse_expr_stmt(parser);
    } else {
      name = parse_var(parser, false);
    }
  }
  parser->func++;
  // params
  consume(parser, TK_LBRACK);
  AstNode* node = new_node(parser);
  node->func = (FuncNode) {
      .type = AST_FUNC,
      .name = name,
      .params_count = 0,
      .line = curr_tok.line};
  AstNode* var_node;
  VarNode* check;
  Token tok;
  while (!is_tty(parser, TK_RBRACK) && !is_tty(parser, TK_EOF)) {
    if (node->func.params_count > 0) {
      consume(parser, TK_COMMA);
    }
    tok = parser->current_tk;
    var_node = parse_var(parser, false);
    // check for duplicate params
    for (int i = 0; i <= node->func.params_count - 1; i++) {
      check = &node->func.params[i]->var;
      if (var_node->var.len == check->len
          && memcmp(var_node->var.name, check->name, var_node->var.len)
              == 0) {
        int len = BUFFER_SIZE + check->len;
        char buff[len];
        snprintf(
            buff,
            len,
            error_types[E0011].err_msg,
            check->len,
            check->name);
        ErrorArgs args = new_error_arg(&tok, buff, NULL);
        return parse_error(parser, E0011, &args);
      }
    }
    node->func.params[node->func.params_count++] = var_node;
    if (node->func.params_count > CONST_MAX) {
      CREATE_BUFFER(buff, error_types[E0012].hlp_msg, CONST_MAX)
      ErrorArgs args = new_error_arg(&tok, NULL, buff);
      return parse_error(parser, E0012, &args);
    }
  }
  consume(parser, TK_RBRACK);
  // body
  node->func.body = parse_block_stmt(parser);
  AstNode* ret_node = set_return(parser, &node->func.body->block_stmt);
  can_tco(&node->func, ret_node);
  parser->func--;
  return node;
}

StructMetaTy get_meta_ty(Token token) {
  // compose
  // declare
  switch (*token.value) {
    case 'c': {
      if (token.length == 7 && memcmp(token.value, "compose", 7) == 0) {
        return SM_COMPOSE;
      }
      break;
    }
    case 'd': {
      if (token.length == 7 && memcmp(token.value, "declare", 7) == 0) {
        return SM_DECLARE;
      }
      break;
    }
  }
  return SM_NIL;
}

AstNode* parse_struct_decl(Parser* parser) {
  /*
   * structDecl     → "struct" IDENTIFIER "{" ( structBody )* "}" ;
     structBody     → "@" structMeta ":"? IDENTIFIER ( "," IDENTIFIER )* ";"
                    | "@" structMeta ":"? IDENTIFIER "=>" expression ( "," IDENTIFIER "=>" expression )* ";" ;
   */
  int line = parser->current_tk.line;
  consume(parser, TK_STRUCT);
  AstNode* st_name = parse_var(parser, false);
  AstNode* node = new_node(parser);
  node->strukt = (StructNode) {
      .type = AST_STRUCT,
      .name = st_name,
      .field_count = 0,
      .line = line};
  StructNode* strukt = &node->strukt;
  consume(parser, TK_LCURLY);
  while (!is_tty(parser, TK_RCURLY) && !is_tty(parser, TK_EOF)) {
    int count = 0;
    consume(parser, TK_AT);
    // handle meta 'compose' | 'declare'
    consume(parser, TK_IDENT);
    StructMetaTy meta_ty = get_meta_ty(parser->previous_tk);
    if (meta_ty == SM_NIL) {
      ErrorArgs args = new_error_arg(&parser->previous_tk, NULL, NULL);
      return parse_error(parser, E0013, &args);
    }
    // colon is optional
    match(parser, TK_COLON);
    // expect identifier
    if (!is_tty(parser, TK_IDENT)) {
      return parse_error(parser, E0014, NULL);
    }
    while (!is_tty(parser, TK_SEMI_COLON) && !is_tty(parser, TK_EOF)) {
      if (count > 0) {
        consume(parser, TK_COMMA);
      }
      Token var = parser->current_tk;
      consume(parser, TK_IDENT);
      strukt->fields[strukt->field_count++] =
          (StructMeta) {.type = meta_ty, .var = var, .expr = NULL};
      if (meta_ty == SM_DECLARE) {
        consume(parser, TK_ARROW);
        strukt->fields[strukt->field_count - 1].expr = parse_expr(parser);
      }
      count++;
      if (strukt->field_count > CONST_MAX) {
        CREATE_BUFFER(buff, error_types[E0015].hlp_msg, CONST_MAX)
        ErrorArgs args = new_error_arg(&var, NULL, buff);
        return parse_error(parser, E0015, &args);
      }
    }
    consume(parser, TK_SEMI_COLON);
  }
  consume(parser, TK_RCURLY);
  // define struct declarations as variable declarations
  AstNode* decl = new_node(parser);
  decl->binary = (BinaryNode) {
      .type = AST_VAR_DECL,
      .l_node = st_name,
      .r_node = node,
      .op = OP_EQ,
      .line = strukt->line};
  return decl;
}

void resync(Parser* parser) {
  parser->panicking = false;
  while (!is_tty(parser, TK_EOF)) {
    switch (parser->current_tk.ty) {
      case TK_LET:
      case TK_SHOW:
      case TK_ASSERT:
      case TK_WHILE:
      case TK_IF:
      case TK_FN:
        return;
      default:
        advance(parser);
    }
  }
}

static AstNode* parse_decls(Parser* parser) {
  if (is_tty(parser, TK_LET)) {
    return parse_var_decl(parser);
  } else if (is_tty(parser, TK_FN)) {
    return parse_func_decl(parser, false);
  } else if (is_tty(parser, TK_STRUCT)) {
    return parse_struct_decl(parser);
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

void cleanup_parser(Parser* parser, char* src) {
  free_parser(parser);
  free(src);
}

#undef CREATE_BUFFER
