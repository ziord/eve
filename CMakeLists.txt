cmake_minimum_required(VERSION 3.23)
project(eve C)

set(CMAKE_C_STANDARD 11)

add_executable(eve main.c value.h ast.h vm.c vm.h code.h code.c memory.h memory.c defs.h common.h util.h util.c debug.c debug.h value.c lexer.c lexer.h parser.c parser.h ast.c errors.c errors.h compiler.c compiler.h gen.c gen.h table.c table.h)
