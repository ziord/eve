#include "memory.h"

#include "gc.h"
#include "vm.h"

void* vm_alloc(
    VM* vm,
    void* ptr,
    size_t curr_size,
    size_t new_size,
    char* fmt,
    ...) {
  vm->gc.bytes_allocated += (new_size - curr_size);
  if (new_size > curr_size) {
#ifdef EVE_DEBUG_STRESS_GC
    collect(vm);
#endif
    if (vm->gc.bytes_allocated > vm->gc.next_collection) {
      collect(vm);
    }
  }
  if (new_size == 0) {
    free(ptr);
#ifdef EVE_DEBUG_GC
    printf("    * deallocated %ld bytes\n", curr_size);
#endif
    return NULL;
  }
  void* tmp = realloc(ptr, new_size);
  if (!tmp) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
  }
#ifdef EVE_DEBUG_GC
  printf("    * allocated %ld bytes\n", new_size - curr_size);
#endif
  return tmp;
}

void* alloc(void *ptr, size_t new_size) {
  void* tmp = realloc(ptr, new_size);
  if (tmp == NULL) {
    FAIL("Allocation Failure", "realloc returned NULL");
  }
  return tmp;
}