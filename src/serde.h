#ifndef EVE_SERDE_H
#define EVE_SERDE_H
#include "vm.h"

typedef enum {
  SD_INIT,
  SD_SERIALIZE,
  SD_DESERIALIZE,
} SerdeMode;

typedef void (*error_cb)(VM* vm, char* fmt, ...);

typedef struct {
  FILE* file;
  SerdeMode mode;
  VM* vm;
  error_cb callback;
} EveSerde;

void init_serde(EveSerde* serde, SerdeMode mode, VM* vm, error_cb cb);
void free_serde(EveSerde* serde);
bool serialize(EveSerde* serde, const char* fname, ObjFn* script);
ObjFn* deserialize(EveSerde* serde, const char* fname);

#endif  //EVE_SERDE_H
