#include <stdio.h>

#include "debug.h"
#include "vm.h"

int main() {
  Code code;
  init_code(&code);
  VM vm;
  Value a = NUMBER_VAL(3);
  Value b = NUMBER_VAL(6);
  Value c = NUMBER_VAL(7);
  Value d = NUMBER_VAL(2);
  // (3 - 6) * 7 / 2
  // load a
  int offset_a = write_value(&code.vpool, a, &vm);
  write_code(&code, $LOAD_CONST, 1, &vm);
  write_code(&code, (byte_t)offset_a, 1, &vm);
  // load b
  int offset_b = write_value(&code.vpool, b, &vm);
  write_code(&code, $LOAD_CONST, 1, &vm);
  write_code(&code, (byte_t)offset_b, 1, &vm);
  // subtract
  write_code(&code, $SUBTRACT, 1, &vm);
  // load c
  int offset_c = write_value(&code.vpool, c, &vm);
  write_code(&code, $LOAD_CONST, 1, &vm);
  write_code(&code, (byte_t)offset_c, 1, &vm);
  // multiply
  write_code(&code, $MULTIPLY, 1, &vm);
  // load d
  int offset_d = write_value(&code.vpool, d, &vm);
  write_code(&code, $LOAD_CONST, 1, &vm);
  write_code(&code, (byte_t)offset_d, 1, &vm);
  // divide
  write_code(&code, $DIVIDE, 1, &vm);
  // return
  write_code(&code, $RET, 2, &vm);
  init_vm(&vm, &code);
  dis_code(&code, "test");
  run(&vm);
  free_code(&code, &vm);
  printf("Hello, World!\n");
  return 0;
}
