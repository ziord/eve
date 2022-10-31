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
  // 6 && 5 || 2
  // "\tfoxy issa \' \t \n nice!"
  // [1, 2, 0xff * 24, 5 * 3 -12, "fishy", [1, 2, 0xff * 24, 5 * 3 -12, "fishy"]]
  // [#{"name": "jon", 5: 0xffff, 888: 5}, #{1: 5}]
  // show #{true: None, None: false, false: None}, 2 / 4 * 0xff, ["yo!"];
  // show #{"ball": 0xb, 2: "b", 3: "c", "dog": [1, 2, 3, #{"a": "apple", "b": "bag"}, "crab"]}["dog"][-2]["a"][2];
  Parser parser = new_parser("show #{1: [1, 2]}[1][-1];");
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
