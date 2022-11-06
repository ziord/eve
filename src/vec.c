#include "vec.h"

void vec_init(Vec* vec) {
  vec->cap = vec->len = 0;
  vec->items = NULL;
}

void vec_push(Vec* vec, void* item) {
  if (vec->len >= vec->cap) {
    vec->cap = GROW_CAPACITY(vec->cap);
    vec->items = alloc(vec->items, vec->cap * sizeof(*vec->items));
  }
  vec->items[vec->len++] = item;
}

inline int vec_size(Vec* vec) {
  return vec->len;
}

void vec_free(Vec* vec) {
  if (!vec->cap)
    return;
  free(vec->items);
  vec_init(vec);
}