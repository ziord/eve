#ifndef EVE_AST_H
#define EVE_AST_H

#include "memory.h"

typedef enum {
  OP_PLUS,
  OP_MINUS,
  OP_MUL,
  OP_DIV,
} OpTy;

typedef enum {
  AST_NUM,
  AST_STR,
  AST_UNARY,
  AST_BINARY,
} AstTy;

typedef union AstNode AstNode;

typedef struct {
  AstTy type;
  int line;
  double value;
} NumberNode;

typedef struct {
  AstTy type;
  OpTy op;
  int line;
  AstNode* l_node;
  AstNode* r_node;
} BinaryNode;

typedef struct {
  AstTy type;
  int line;
  OpTy op;
  AstNode* node;
} UnaryNode;

typedef struct {
  int length;
  int capacity;
  AstNode** nodes;
} NodeStore;

union AstNode {
  NumberNode num;
  BinaryNode binary;
  UnaryNode unary;
};

void init_store(NodeStore* store);
void free_store(NodeStore* store);
AstNode* new_node(NodeStore* store);
AstNode* new_num(NodeStore* store, double val, int line);
AstNode* new_binary(
    NodeStore* store,
    AstNode* left,
    AstNode* right,
    int line,
    OpTy op);

#endif  //EVE_AST_H
