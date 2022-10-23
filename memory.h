#ifndef EVE_MEMORY_H
#define EVE_MEMORY_H
#include "common.h"
#include "defs.h"

#define GROW_CAPACITY(cap) ((cap) < 8 ? 8 : ((cap) << 2))
#define GROW_BUFFER(vm, ptr, type, o_size, n_size)                \
  ((type *) alloc(vm, ptr, (sizeof(type) * (o_size)),             \
                  (sizeof(type) * (n_size)), "buffer grow failed", \
                  __FILE__, __LINE__))

#define ALLOC(vm, type, size)                           \
  ((type *) alloc(vm, NULL, 0, (sizeof(type) * (size)), \
                  "allocation failed", __FILE__, __LINE__))
#define FREE(vm, ptr, type) alloc(vm, ptr, sizeof(type), 0, "")

void *alloc(VM *vm, void *ptr, size_t curr_size, size_t new_size,
            char *fmt, ...);

#endif//EVE_MEMORY_H
