cmake_minimum_required(VERSION 3.23)
project(eve C)

set(CMAKE_C_STANDARD 11)

add_executable(eve main.c value.h ast.h vm.c vm.h code.h code.c memory.h memory.c defs.h common.h util.h util.c debug.c debug.h value.c)
