#ifndef EVE_AST_H
#define EVE_AST_H

#include "lexer.h"
#include "memory.h"
#include "vec.h"

typedef enum {
  OP_PLUS,
  OP_MINUS,
  OP_MUL,
  OP_DIV,
  OP_POW,
  OP_MOD,
  OP_EQ,
  OP_LESS,
  OP_LESS_EQ,
  OP_GRT,
  OP_GRT_EQ,
  OP_NOT_EQ,
  OP_LOG_EQ,
  OP_LSHIFT,
  OP_RSHIFT,
  OP_OR,
  OP_AND,
  OP_BW_AND,
  OP_BW_XOR,
  OP_BW_OR,
  OP_NOT,
  OP_BW_COMPL,
  OP_DCOL,
  OP_DOT,
} OpTy;

typedef enum {
  AST_NUM = 1,
  AST_STR,
  AST_LIST,
  AST_MAP,
  AST_UNIT,
  AST_VAR,
  AST_TRY,
  AST_THROW,
  AST_ASSIGN,
  AST_ERROR,
  AST_UNARY,
  AST_BINARY,
  AST_SUBSCRIPT,
  AST_EXPR_STMT,
  AST_SHOW_STMT,
  AST_ASSERT_STMT,
  AST_BLOCK_STMT,
  AST_IF_STMT,
  AST_WHILE_STMT,
  AST_FOR_STMT,
  AST_LOOP,
  AST_CONTROL_STMT,
  AST_RETURN_STMT,
  AST_VAR_DECL,
  AST_FUNC,
  AST_STRUCT,
  AST_STRUCT_CALL,
  AST_CALL,
  AST_DOT_EXPR,
  AST_PROGRAM,
} AstTy;

typedef union AstNode AstNode;

typedef struct {
  AstTy type;
  int line;
  double value;
} NumberNode;

typedef struct {
  AstTy type;
  bool is_alloc;
  int line;
  int length;
  char* start;
} StringNode;

typedef struct {
  AstTy type;
  int line;
  int len;
  AstNode* elems[CONST_MAX];
} ListNode;

typedef struct {
  AstTy type;
  int line;
  int length;
  AstNode* items[CONST_MAX][2];  // key-value pairs
} MapNode;

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
  // bool, None
  AstTy type;
  bool is_bool;
  bool is_none;
  bool is_true_bool;
  bool is_false_bool;
  int line;
} UnitNode;

typedef struct {
  AstTy type;
  int line;
  AstNode* expr;
  AstNode* subscript;
} SubscriptNode;

typedef struct {
  AstTy type;
  int line;
  AstNode* expr;
} ExprStmtNode;

typedef struct {
  AstTy type;
  int line;
  AstNode* test;
  AstNode* msg;
} AssertStmtNode;

typedef struct {
  AstTy type;
  int line;
  int length;
  AstNode* items[CONST_MAX];
} ShowStmtNode;

typedef struct {
  AstTy type;
  int line;
  Vec stmts;
} BlockStmtNode;

typedef struct {
  AstTy type;
  int line;
  AstNode* condition;
  AstNode* if_block;
  AstNode* else_block;
} IfElseStmtNode;

typedef struct {
  AstTy type;
  int line;
  int len;
  char* name;
} VarNode;

typedef struct {
  AstTy type;
  int line;
  int patch_slot;
  bool is_continue;
  bool is_break;
} ControlStmtNode;

typedef struct {
  AstTy type;
  int line;
  AstNode* condition;
  AstNode* block;
} WhileStmtNode;

typedef struct {
  AstTy type;
  int line;
  AstNode* elem;
  AstNode* iterable;
  AstNode* block;
  AstNode* desugar;
} ForStmtNode;

typedef struct {
  AstTy type;
  int line;
  int params_count;
  AstNode* params[CONST_MAX];
  AstNode* name;  // VarNode
  AstNode* body;  // BlockStmtNode
} FuncNode;

typedef struct {
  AstTy type;
  bool is_tail_call;
  int line;
  int args_count;
  AstNode* args[CONST_MAX];
  AstNode* left;
} CallNode;

typedef enum {
  SM_NIL,
  SM_COMPOSE,
  SM_DECLARE,
} StructMetaTy;

typedef struct {
  StructMetaTy type;
  Token var;
  AstNode* expr;  // may be NULL, depending on type
} StructMeta;

typedef struct {
  AstTy type;
  int line;
  int field_count;
  AstNode* name;
  StructMeta fields[CONST_MAX];
} StructNode;

typedef struct {
  AstTy type;
  int line;
  AstNode* name;
  MapNode fields;
} StructCallNode;

typedef struct {
  AstTy type;
  int line;
  AstNode* try_expr;
  AstNode* try_var;
  AstNode* else_expr;
} TryNode;

typedef struct {
  AstTy type;
  Vec decls;
} ProgramNode;

union AstNode {
  NumberNode num;
  BinaryNode binary;
  UnaryNode unary;
  UnitNode unit;
  StringNode str;
  ListNode list;
  MapNode map;
  VarNode var;
  FuncNode func;
  CallNode call;
  StructNode strukt;
  SubscriptNode subscript;
  ExprStmtNode expr_stmt;
  ShowStmtNode show_stmt;
  AssertStmtNode assert_stmt;
  BlockStmtNode block_stmt;
  IfElseStmtNode ife_stmt;
  WhileStmtNode while_stmt;
  ForStmtNode for_stmt;
  ControlStmtNode control_stmt;
  StructCallNode struct_call;
  TryNode try_h;
  ProgramNode program;
};

typedef struct {
  Vec nodes;
} NodeStore;

void init_store(NodeStore* store);
void free_store(NodeStore* store);
OpTy get_op(TokenTy ty);
AstNode* new_ast_node(NodeStore* store);
AstNode* new_num(NodeStore* store, double val, int line);
AstNode* new_none(NodeStore* store, int line);
AstNode* new_unary(NodeStore* store, AstNode* node, int line, OpTy op);
AstNode* new_var(NodeStore* store, Token token);
AstNode* new_binary(
    NodeStore* store,
    AstNode* left,
    AstNode* right,
    int line,
    OpTy op);

#endif  //EVE_AST_H
