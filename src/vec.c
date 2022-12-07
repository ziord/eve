#include "vec.h"

void vec_init(Vec* vec) {
  vec->cap = vec->len = 0;
  vec->items = NULL;
}

void vec_push(Vec* vec, void* item) {
  if (!vec || !item)
    return;
  if (vec->len >= vec->cap) {
    vec->cap = GROW_CAPACITY(vec->cap);
    vec->items = alloc(vec->items, vec->cap * sizeof(*vec->items));
  }
  vec->items[vec->len++] = item;
}

void vec_extend(Vec* vec1, Vec* vec2) {
  if (!vec1 || !vec2 || !vec1->len || !vec2->len)
    return;
  for (int i = 0; i < vec2->len; i++) {
    vec_push(vec1, vec2->items[i]);
  }
}

inline void* vec_pop(Vec* vec) {
  if (!vec || !vec->len)
    return NULL;
  return vec->items[--vec->len];
}

inline int vec_size(Vec* vec) {
  // assumes vec is not NULL
  return vec->len;
}

void vec_free(Vec* vec) {
  // assumes vec is not NULL
  if (!vec->cap)
    return;
  free(vec->items);
  vec_init(vec);
}