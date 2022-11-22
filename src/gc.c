#include "gc.h"

#include "compiler.h"
#include "vm.h"

void mark_value(VM* vm, Value v);
void mark_object(VM* vm, Obj* obj);

void gc_init(GC* gc) {
  gc->bytes_allocated = 0;
  gc->next_collection = 0;
  vec_init(&gc->gray_stack);
}

void gc_free(GC* gc) {
  vec_free(&gc->gray_stack);
  gc_init(gc);
}

void free_object(VM* vm, Obj* obj) {
#if defined(EVE_DEBUG_GC)
  printf("  [*] free %p type %d\n", obj, obj->type);
#endif
  switch (obj->type) {
    case OBJ_STR: {
      ObjString* st = (ObjString*)obj;
      FREE_BUFFER(vm, st->str, char, st->length + 1);
      FREE(vm, st, ObjString);
      break;
    }
    case OBJ_LIST: {
      ObjList* list = (ObjList*)obj;
      size_t size =
          sizeof(ObjList) + (sizeof(Value) * list->elems.capacity);
      FREE_FLEX(vm, list, size);
      break;
    }
    case OBJ_HMAP: {
      ObjHashMap* map = (ObjHashMap*)obj;
      FREE_BUFFER(vm, map->entries, HashEntry, map->capacity);
      FREE(vm, map, ObjHashMap);
      break;
    }
    case OBJ_FN: {
      ObjFn* func = (ObjFn*)obj;
      free_code(&func->code, vm);
      FREE(vm, func, ObjFn);
      break;
    }
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)obj;
      FREE_BUFFER(vm, closure->env, ObjUpvalue*, closure->env_len);
      FREE(vm, closure, ObjClosure);
      break;
    }
    case OBJ_UPVALUE: {
      FREE(vm, obj, ObjUpvalue);
      break;
    }
    case OBJ_MODULE:
    case OBJ_STRUCT: {
      ObjStruct* st = (ObjStruct*)obj;
      FREE_BUFFER(vm, st->fields.entries, HashEntry, st->fields.capacity);
      FREE(vm, obj, ObjStruct);
      break;
    }
    case OBJ_INSTANCE: {
      ObjInstance* ins = (ObjInstance*)obj;
      FREE_BUFFER(vm, ins->fields.entries, HashEntry, ins->fields.capacity);
      FREE(vm, obj, ObjInstance);
      break;
    }
    case OBJ_CFN:
      FREE(vm, obj, ObjCFn);
      break;
  }
}

void remove_whites(ObjHashMap* map) {
  HashEntry* entry;
  // vm->strings is a hashmap of {ObjString*, FALSE_VAL}, basically a set
  for (int i = 0; i < map->capacity; i++) {
    entry = &map->entries[i];
    // all unmarked strings are whites (unreachable), so remove them.
    if (IS_OBJ(entry->key) && !AS_OBJ(entry->key)->marked) {
#ifdef EVE_DEBUG_GC
      printf("  [*] removing map weak-ref %p (", AS_OBJ(entry->key));
      print_value(entry->key);
      printf(")\n");
#endif
      hashmap_remove(map, entry->key);
    }
  }
}

void mark_hashmap(VM* vm, ObjHashMap* map) {
  HashEntry* entry;
  for (int i = 0; i < map->capacity; i++) {
    entry = &map->entries[i];
    if (!IS_NOTHING(entry->key)) {
      mark_value(vm, entry->key);
      mark_value(vm, entry->value);
    }
  }
}

void mark_compiler(VM* vm) {
  struct Compiler* compiler = vm->compiler;
  while (compiler != NULL) {
    mark_object(vm, &compiler->func->obj);
    compiler = compiler->enclosing;
  }
}

void mark_object(VM* vm, Obj* obj) {
  if (!obj || obj->marked)
    return;
#if defined(EVE_DEBUG_GC)
  printf("   * mark object %p (", obj);
  print_value(OBJ_VAL(obj));
  printf(")\n");
#endif
  obj->marked = true;
  // add gray
  vec_push(&vm->gc.gray_stack, obj);
}

void mark_value(VM* vm, Value v) {
  if (IS_OBJ(v)) {
    mark_object(vm, AS_OBJ(v));
  }
}

void mark_roots(VM* vm) {
#if defined(EVE_DEBUG_GC)
  printf("  [*] -- marking roots\n");
#endif
  // mark stack roots
  for (Value* v = vm->stack; v < vm->sp; v++) {
    mark_value(vm, *v);
  }
  // mark upvalue roots
  for (ObjUpvalue* upvalue = vm->upvalues; upvalue != NULL;
       upvalue = upvalue->next) {
    mark_object(vm, &upvalue->obj);
  }
  // mark call-frame roots
  for (int i = 0; i < vm->frame_count; i++) {
    mark_object(vm, &vm->frames[i].closure->obj);
  }
  // mark globals roots
  mark_hashmap(vm, &vm->fp->closure->func->module->fields);
  // mark modules roots
  mark_hashmap(vm, &vm->modules);
  // mark compiler roots
  mark_compiler(vm);
#if defined(EVE_DEBUG_GC)
  printf("  [*] -- done marking roots\n");
#endif
}

void blacken_object(VM* vm, Obj* obj) {
#if defined(EVE_DEBUG_GC)
  printf("  [*] -- blacken object %p (", obj);
  print_value(OBJ_VAL(obj));
  printf(")\n");
#endif
  switch (obj->type) {
    case OBJ_LIST: {
      ObjList* list = (ObjList*)obj;
      for (int i = 0; i < list->elems.length; i++) {
        mark_value(vm, list->elems.buffer[i]);
      }
      return;
    }
    case OBJ_HMAP: {
      mark_hashmap(vm, (ObjHashMap*)obj);
      return;
    }
    case OBJ_FN: {
      ObjFn* fn = (ObjFn*)obj;
      for (int i = 0; i < fn->code.vpool.length; i++) {
        mark_value(vm, fn->code.vpool.values[i]);
      }
      if (fn->name) {
        mark_object(vm, &fn->name->obj);
      }
      mark_object(vm, &fn->module->obj);
      return;
    }
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)obj;
      mark_object(vm, &closure->func->obj);
      for (int i = 0; i < closure->env_len; i++) {
        mark_object(vm, &closure->env[i]->obj);
      }
      return;
    }
    case OBJ_UPVALUE: {
      mark_value(vm, ((ObjUpvalue*)obj)->value);
      return;
    }
    case OBJ_MODULE:
    case OBJ_STRUCT: {
      ObjStruct* strukt = (ObjStruct*)obj;
      mark_hashmap(vm, &strukt->fields);
      mark_object(vm, &strukt->name->obj);
      return;
    }
    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)obj;
      mark_hashmap(vm, &instance->fields);
      mark_object(vm, &instance->strukt->obj);
      return;
    }
    case OBJ_CFN:
    case OBJ_STR:
      return;
  }
  UNREACHABLE("blacken-object");
}

void trace_references(VM* vm) {
  while (vec_size(&vm->gc.gray_stack)) {
    blacken_object(vm, vec_pop(&vm->gc.gray_stack));
  }
}

void sweep(VM* vm) {
#ifdef EVE_DEBUG_GC
  printf("  [*] begin sweep\n");
#endif
  for (Obj *curr = vm->objects, *prev = NULL; curr != NULL;) {
    if (curr->marked) {
      curr->marked = false;
      prev = curr;
      curr = curr->next;
    } else {
      Obj* garbage = curr;
      curr = curr->next;
      if (prev != NULL) {
        // set next of prev to the new curr,
        // essentially deleting the old curr
        prev->next = curr;
      } else {
        // if we end up here, it means we're freeing the head pointer
        // so reset it here.
        vm->objects = curr;
      }
      free_object(vm, garbage);
    }
  }
#ifdef EVE_DEBUG_GC
  printf("  [*] end sweep\n");
#endif
}

void collect(VM* vm) {
  // do not collect during compilation
  if (vm->is_compiling)
    return;
#ifdef EVE_DEBUG_GC
  printf("[*] begin collection\n");
  size_t size_before = vm->gc.bytes_allocated;
#endif
  // mark-roots
  mark_roots(vm);
  // trace-references
  trace_references(vm);
  // remove weak-refs
  remove_whites(&vm->strings);
  // sweep
  sweep(vm);
  vm->gc.next_collection = vm->gc.bytes_allocated * GC_HEAP_GROWTH_FACTOR;
#if defined(EVE_DEBUG_GC)
  size_t size_now = vm->gc.bytes_allocated;
  printf(
      "  [*] collected %zu bytes (from %zu to %zu), next collection at %zu\n",
      size_before - size_now,
      size_before,
      size_now,
      vm->gc.next_collection);
  printf("[*] end collection\n");
#endif
}
