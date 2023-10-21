#include <stdio.h>

#include "compiler.h"
#include "serde.h"
#include "vm.h"
//#ifdef EVE_DEBUG
#include "debug.h"
//#endif

int execute_eve(char* fp, const char* bin, bool dis) {
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
  if (bin) {
    EveSerde serde;
    init_serde(&serde, SD_SERIALIZE, &vm, (error_cb)serde_error_cb);
    serialize(&serde, bin, func);
    free_serde(&serde);
  }
  if (dis) {
    dis_code(&func->code, "<debug>");
  }
  // run
  boot_vm(&vm, func);
  IResult ret = run(&vm);
  // destruct
  free(src);
  free_vm(&vm);
  return ret;
}

int execute_eco(char* fp) {
  VM vm = new_vm();
  EveSerde serde;
  init_serde(&serde, SD_DESERIALIZE, &vm, (error_cb)serde_error_cb);
  ObjFn* de_fun = deserialize(&serde, fp);
  vm.is_compiling = true;
  Compiler compiler;
  new_compiler(&compiler, NULL, de_fun, &vm, NULL);
#ifdef EVE_DEBUG
  dis_code(&de_fun->code, get_func_name(de_fun));
#endif
  boot_vm(&vm, de_fun);
  IResult ret = run(&vm);
  free_vm(&vm);
  return ret;
}

int show_options() {
  printf("Usage: eve [-h | --help] [-v | --version] -d? <input-file>\n");
  return 0;
}

int show_help() {
  printf("This is the Eve interpreter (version %s).\n", EVE_VERSION);
  printf("To run a program 'file.eve', do:\n");
  printf("\teve file.eve\n");
  printf("To view other options, simply use eve\n");
  return 0;
}

int show_version() {
  printf("Eve %s\n", EVE_VERSION);
  return 0;
}

int parse_args(int argc, char* argv[]) {
  if (argc < 2) {
    return show_options();
  }
  switch (*(argv[1])) {
    case '-': {
      if (strlen(argv[1]) == 2 && argv[1][1] == 'd') {
        return execute_eve(argv[2], NULL, true);
      } else if (
          strlen(argv[1]) == 2 && argv[1][1] == 'h'
          || strncmp(argv[1], "--help", 6) == 0) {
        return show_help();
      } else if (
          strlen(argv[1]) == 2 && argv[1][1] == 'v'
          || strncmp(argv[1], "--version", 9) == 0) {
        return show_version();
      } else {
        fputs("Err. Invalid arguments.\n", stderr);
        return show_options();
      }
    }
    default:
      return execute_eve(argv[1], NULL, false);
  }
}

int main(int argc, char* argv[]) {
  (void)execute_eco;
  return parse_args(argc, argv);
}
