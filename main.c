#include <stdio.h>

#include "src/compiler.h"
#include "src/debug.h"
#include "src/vm.h"

int execute(char* fp) {
  Code code;
  init_code(&code);
  VM vm = new_vm();
  // parse
  char* src = read_file(fp);
  Parser parser = new_parser(src, fp);
  AstNode* root = parse(&parser);
  if (parser.errors) {
    free_parser(&parser);
    return RESULT_COMPILE_ERROR;
  }
  // compile
  Compiler compiler = new_compiler(root, &code, &vm);
  compile(&compiler);
  dis_code(&code, "test");
  free_parser(&parser);
  // run
  boot_vm(&vm, &code);
  IResult ret = run(&vm);
  // destruct
  free_code(&code, &vm);
  free(src);
  return ret;
}

int show_options() {
  printf("Usage: eve [-h <help>] <input-file>\n");
  return 0;
}

int show_help() {
  printf("This is the Eve interpreter.\n");
  printf("To run a program 'file.eve', do:\n");
  printf("\teve file.eve\n");
  printf("To view other options, simply use eve\n");
  return 0;
}

int parse_args(int argc, char* argv[]) {
  if (argc != 2) {
    return show_options();
  }
  switch (*(argv[1])) {
    case '-': {
      if (strlen(argv[1]) == 2 && argv[1][1] == 'h') {
        return show_help();
      } else {
        fputs("Err. Invalid arguments.\n", stderr);
        return show_options();
      }
    }
    default:
      return execute(argv[1]);
  }
}

int main(int argc, char* argv[]) {
  return parse_args(argc, argv);
}
