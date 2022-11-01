#ifndef EVE_VEC_H
#define EVE_VEC_H

#include "memory.h"

typedef struct {
  int len;
  int cap;
  void** items;
} Vec;

void vec_init(Vec* vec);
void vec_free(Vec* vec);
void vec_push(Vec* vec, void* item);

#endif  //EVE_VEC_H
