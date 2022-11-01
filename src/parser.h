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
  char* file_path;
  NodeStore store;
} Parser;

Parser new_parser(char* src, char* fp);
void free_parser(Parser* parser);
AstNode* parse(Parser* parser);

#endif  //EVE_PARSER_H
