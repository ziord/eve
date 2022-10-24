#ifndef EVE_PARSER_H
#define EVE_PARSER_H

#include "ast.h"
#include "errors.h"
#include "lexer.h"
#include "util.h"

typedef struct {
  Token current_tk;
  Token previous_tk;
  Lexer lexer;
  NodeStore store;
} Parser;

Parser new_parser(char* src);
AstNode* parse(Parser* parser);

#endif  //EVE_PARSER_H
