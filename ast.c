#include "ast.h"

void init_store(NodeStore* store) {
  store->capacity = store->length = 0;
  store->nodes = NULL;
}

void free_store(NodeStore* store) {
  for (int i = 0; i < store->length; i++) {
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
