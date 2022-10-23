#ifndef EVE_AST_H
#define EVE_AST_H

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

typedef struct {
  AstTy type;
  double value;
} NumberNode;

typedef struct {
  AstTy type;
  OpTy op;
  void *l_node;
  void *r_node;
} BinaryNode;

typedef struct {
  AstTy type;
  OpTy op;
  void *node;
} UnaryNode;
#endif //EVE_AST_H
