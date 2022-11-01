cmake_minimum_required(VERSION 3.23)
project(eve C)

set(CMAKE_C_STANDARD 11)

add_executable(eve main.c src/value.h src/ast.h src/vm.c src/vm.h src/code.h src/code.c src/memory.h src/memory.c src/defs.h src/common.h src/util.h src/util.c src/debug.c src/debug.h src/value.c src/lexer.c src/lexer.h src/parser.c src/parser.h src/ast.c src/errors.c src/errors.h src/compiler.c src/compiler.h src/gen.c src/gen.h src/vec.c src/vec.h)
