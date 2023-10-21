#ifndef EVE_GC_H
#define EVE_GC_H
#include "memory.h"
#include "value.h"
#include "vec.h"

#define GC_HEAP_GROWTH_FACTOR 1

typedef struct {
  // total bytes allocated
  size_t bytes_allocated;
  // number of bytes which would trigger next collection.
  size_t next_collection;
  // vec for storing gray objects
  Vec gray_stack;
} GC;

void gc_init(GC* gc);
void gc_free(GC* gc);
void collect(VM* vm);

#endif  //EVE_GC_H
