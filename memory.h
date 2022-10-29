#ifndef EVE_MEMORY_H
#define EVE_MEMORY_H
#include "defs.h"
#include "util.h"

#define BUFFER_INIT_SIZE (8)
#define GROW_CAPACITY(cap) \
  ((cap) < BUFFER_INIT_SIZE ? BUFFER_INIT_SIZE : ((cap) << 2))

#define GROW_BUFFER(vm, ptr, type, o_size, n_size) \
  ((type*)vm_alloc( \
      vm, \
      ptr, \
      (sizeof(type) * (o_size)), \
      (sizeof(type) * (n_size)), \
      "buffer grow failed", \
      __FILE__, \
      __LINE__))

#define ALLOC(vm, type, size) \
  ((type*)vm_alloc( \
      vm, \
      NULL, \
      0, \
      (sizeof(type) * (size)), \
      "allocation failed", \
      __FILE__, \
      __LINE__))

#define FREE(vm, ptr, type) vm_alloc(vm, ptr, sizeof(type), 0, "")

void* vm_alloc(
    VM* vm,
    void* ptr,
    size_t curr_size,
    size_t new_size,
    char* fmt,
    ...);

void* alloc(void *ptr, size_t new_size);

#endif  //EVE_MEMORY_H
