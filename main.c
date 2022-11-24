#include <stdio.h>

#include "src/compiler.h"
#include "src/debug.h"
#include "src/vm.h"

int execute(char* fp) {
  VM vm = new_vm();
  // parse
  char* src = NULL;
  char* msg = read_file(fp, &src);
  if (msg != NULL) {
    fprintf(stderr, "%s\n", msg);
    src ? free(src) : (void)0;
    return RESULT_COMPILE_ERROR;
  }
  Parser parser = new_parser(src, fp);
  AstNode* root = parse(&parser);
  if (parser.errors) {
    cleanup_parser(&parser, src);
    return RESULT_COMPILE_ERROR;
  }
  // compile
  ObjFn* func = create_function(&vm);
  Compiler compiler;
  new_compiler(&compiler, root, func, &vm, fp);
  compile(&compiler);
  if (compiler.errors) {
    cleanup_parser(&parser, src);
    return RESULT_COMPILE_ERROR;
  }
  free_parser(&parser);
  // run
  boot_vm(&vm, func);
  IResult ret = run(&vm);
  // destruct
  free(src);
  free_vm(&vm);
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
