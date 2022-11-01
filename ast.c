#include "ast.h"

void init_store(NodeStore* store) {
  store->capacity = store->length = 0;
  store->nodes = NULL;
}

void free_store(NodeStore* store) {
  for (int i = 0; i < store->length; i++) {
    // cleanup all uses of vec
    if (store->nodes[i]->num.type == AST_PROGRAM) {
      vec_free(&store->nodes[i]->program.decls);
    }
    free(store->nodes[i]);
  }
}

void save_node(NodeStore* store, AstNode* node) {
  if (store->length >= store->capacity) {
    store->capacity = GROW_CAPACITY(store->capacity);
    store->nodes = alloc(store->nodes, sizeof(AstNode*) * store->capacity);
  }
  store->nodes[store->length++] = node;
}

OpTy get_op(TokenTy ty) {
  switch (ty) {
    case TK_PLUS:
      return OP_PLUS;
    case TK_MINUS:
      return OP_MINUS;
    case TK_STAR:
      return OP_MUL;
    case TK_STAR_STAR:
      return OP_POW;
    case TK_EXC_MARK:
      return OP_NOT;
    case TK_TILDE:
      return OP_BW_COMPL;
    case TK_FSLASH:
      return OP_DIV;
    case TK_PERCENT:
      return OP_MOD;
    case TK_LESS:
      return OP_LESS;
    case TK_GRT:
      return OP_GRT;
    case TK_LESS_EQ:
      return OP_LESS_EQ;
    case TK_GRT_EQ:
      return OP_GRT_EQ;
    case TK_NOT_EQ:
      return OP_NOT_EQ;
    case TK_EQ_EQ:
      return OP_LOG_EQ;
    case TK_LSHIFT:
      return OP_LSHIFT;
    case TK_RSHIFT:
      return OP_RSHIFT;
    case TK_PIPE:
      return OP_BW_OR;
    case TK_PIPE_PIPE:
      return OP_OR;
    case TK_CARET:
      return OP_BW_XOR;
    case TK_AMP:
      return OP_BW_AND;
    case TK_AMP_AMP:
      return OP_AND;
    default:
      return 0xff;
  }
}

AstNode* new_node(NodeStore* store) {
  AstNode* node = alloc(NULL, sizeof(AstNode));
  save_node(store, node);
  return node;
}

AstNode* new_num(NodeStore* store, double val, int line) {
  AstNode* node = new_node(store);
  node->num = (NumberNode) {.value = val, .type = AST_NUM, .line = line};
  return node;
}

AstNode* new_binary(
    NodeStore* store,
    AstNode* left,
    AstNode* right,
    int line,
    OpTy op) {
  AstNode* node = new_node(store);
  BinaryNode* binary = CAST(BinaryNode*, node);
  binary->type = AST_BINARY;
  binary->l_node = left;
  binary->r_node = right;
  binary->line = line;
  binary->op = op;
  return node;
}

AstNode* new_unary(NodeStore* store, AstNode* node, int line, OpTy op) {
  AstNode* ast_node = new_node(store);
  UnaryNode* unary = CAST(UnaryNode*, ast_node);
  unary->type = AST_UNARY;
  unary->node = node;
  unary->line = line;
  unary->op = op;
  return ast_node;
}