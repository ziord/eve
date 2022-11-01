#include <stdio.h>

#include "compiler.h"
#include "debug.h"
#include "vm.h"

int main() {
  Code code;
  init_code(&code);
  VM vm = new_vm();
  // parse
  char* fp = "../tests/test.eve";
  char* src = read_file(fp);
  Parser parser = new_parser(src, fp);
  AstNode* root = parse(&parser);
  // compile
  Compiler compiler = new_compiler(root, &code, &vm);
  compile(&compiler);
  dis_code(&code, "test");
  free_parser(&parser);
  // run
  init_vm(&vm, &code);
  run(&vm);
  // destruct
  free_code(&code, &vm);
  free(src);
  return 0;
}
