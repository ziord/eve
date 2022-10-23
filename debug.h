#ifndef EVE_DEBUG_H
#define EVE_DEBUG_H
#include "code.h"

int plain_instruction(char* inst, int offset);
int constant_instruction(char* inst, Code* code, int offset);
int dis_instruction(Code* code, int index);
void dis_code(Code* code, char* name);

#endif  //EVE_DEBUG_H
