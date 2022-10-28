#include <stdio.h>

#include "compiler.h"
#include "debug.h"
#include "vm.h"

int main() {
  Code code;
  init_code(&code);
  VM vm = new_vm();
  // "5e2 - 0x6af * 7.67 / 2" | "2 * 4 ** 3" | "2 % 4 -- 1"
  // --++--2
  // 5e2 - 0x6af * 7.67 / 2 % 2 * 4 ** 3 + 2 % 4 -- 1
  // 16 | 3 * 5 & 5e2 - 0x6af * 7.67 / 2 % 2 * 4 ** 3 ^ 2 + 2 % 4 - - 1
  Parser parser = new_parser("6 && 5 || 2");
  AstNode* root = parse(&parser);
  Compiler compiler = new_compiler(root, &code, &vm);
  compile(&compiler);
  dis_code(&code, "test");
  free_store(&parser.store);
  init_vm(&vm, &code);
  run(&vm);
  free_code(&code, &vm);
  printf("Hello, World!\n");
  return 0;
}
