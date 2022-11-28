#include "core.h"

#include <limits.h>
#include <sys/stat.h>
#include <time.h>

#include "compiler.h"
#include "parser.h"
#include "serde.h"
#include "vm.h"

#ifdef _WIN32
  #include <direct.h>
  #define PATH_SEP '\\'
  #define mkdir(dir, mode) _mkdir(dir)
#else
  #define PATH_SEP '/'
#endif

struct FnData {
  char* name;
  int arity;
  CFn func;
};

struct ModuleData {
  char* module_name;
  int name_len;
  int field_len;
  struct FnData data[CONST_MAX];
};

/*** General ***/
Value fn_print(VM* vm, int argc, const Value* args);
Value fn_println(VM* vm, int argc, const Value* args);
Value fn_import(VM* vm, int argc, const Value* args);
Value fn_exit(VM* vm, int argc, const Value* args);
Value fn_type(VM* vm, int argc, const Value* args);
Value fn_clock(VM* vm, int argc, const Value* args);
Value fn_time(VM* vm, int argc, const Value* args);
Value fn_offload(VM* vm, int argc, const Value* args);

/*** string ***/
Value fn_str_len(VM* vm, int argc, const Value* args);
Value fn_str_to_string(VM* vm, int argc, const Value* args);
Value fn_str_append(VM* vm, int argc, const Value* args);
Value fn_str_upper(VM* vm, int argc, const Value* args);
Value fn_str_lower(VM* vm, int argc, const Value* args);
Value fn_str_startswith(VM* vm, int argc, const Value* args);
Value fn_str_endswith(VM* vm, int argc, const Value* args);
Value fn_str_strip(VM* vm, int argc, const Value* args);
Value fn_str_lstrip(VM* vm, int argc, const Value* args);
Value fn_str_rstrip(VM* vm, int argc, const Value* args);
Value fn_str_split(VM* vm, int argc, const Value* args);
Value fn_str_join(VM* vm, int argc, const Value* args);
Value fn_str_index(VM* vm, int argc, const Value* args);
Value fn_str_isaplha(VM* vm, int argc, const Value* args);
Value fn_str_isalnum(VM* vm, int argc, const Value* args);
Value fn_str_isdigit(VM* vm, int argc, const Value* args);
Value fn_str_isdecimal(VM* vm, int argc, const Value* args);
Value fn_str_find(VM* vm, int argc, const Value* args);
Value fn_str_count(VM* vm, int argc, const Value* args);

/*** list ***/
Value fn_list_len(VM* vm, int argc, const Value* args);
Value fn_list_append(VM* vm, int argc, const Value* args);
Value fn_list_pop(VM* vm, int argc, const Value* args);
Value fn_list_clear(VM* vm, int argc, const Value* args);
Value fn_list_copy(VM* vm, int argc, const Value* args);
Value fn_list_extend(VM* vm, int argc, const Value* args);
Value fn_list_index(VM* vm, int argc, const Value* args);
Value fn_list_count(VM* vm, int argc, const Value* args);
Value fn_list_insert(VM* vm, int argc, const Value* args);
Value fn_list_remove(VM* vm, int argc, const Value* args);
Value fn_list_reverse(VM* vm, int argc, const Value* args);
Value fn_list_sort(VM* vm, int argc, const Value* args);

/*** hashmap ***/
Value fn_map_put(VM* vm, int argc, const Value* args);
Value fn_map_del(VM* vm, int argc, const Value* args);
Value fn_map_get(VM* vm, int argc, const Value* args);
Value fn_map_keys(VM* vm, int argc, const Value* args);
Value fn_map_values(VM* vm, int argc, const Value* args);
Value fn_map_items(VM* vm, int argc, const Value* args);
Value fn_map_pop(VM* vm, int argc, const Value* args);
Value fn_map_popitem(VM* vm, int argc, const Value* args);
Value fn_map_clear(VM* vm, int argc, const Value* args);
Value fn_map_copy(VM* vm, int argc, const Value* args);
Value fn_map_setdefault(VM* vm, int argc, const Value* args);
Value fn_map_len(VM* vm, int argc, const Value* args);

/*** struct ***/
Value fn_struct_fields(VM* vm, int argc, const Value* args);
Value fn_struct_has_field(VM* vm, int argc, const Value* args);
Value fn_struct_get_field(VM* vm, int argc, const Value* args);

/*** instance ***/
Value fn_instance_props(VM* vm, int argc, const Value* args);
Value fn_instance_has_prop(VM* vm, int argc, const Value* args);
Value fn_instance_get_prop(VM* vm, int argc, const Value* args);
Value fn_instance_set_prop(VM* vm, int argc, const Value* args);
Value fn_instance_del_prop(VM* vm, int argc, const Value* args);

/*** module ***/
Value fn_module_path(VM* vm, int argc, const Value* args);
Value fn_module_globals(VM* vm, int argc, const Value* args);

struct ModuleData mod_data[] = {
    {.module_name = "core",
     .name_len = 4,
     .field_len = 8,
     .data =
         {
             // negative arity indicates varargs
             {.name = "print", .arity = -1, .func = fn_print},
             {.name = "println", .arity = -1, .func = fn_println},
             {.name = "import", .arity = 1, .func = fn_import},
             {.name = "exit", .arity = 1, .func = fn_exit},
             {.name = "type", .arity = 1, .func = fn_type},
             {.name = "clock", .arity = 0, .func = fn_clock},
             {.name = "time", .arity = 0, .func = fn_time},
             {.name = "offload", .arity = 0, .func = fn_offload},
         }},
    {.module_name = "string",
     .name_len = 6,
     .field_len = 1,
     .data =
         {
             {.name = "len", .arity = 1, .func = fn_str_len},
         }},
    {.module_name = "list",
     .name_len = 4,
     .field_len = 1,
     .data =
         {
             {.name = "len", .arity = 1, .func = fn_list_len},
         }},
    {.module_name = "hashmap",
     .name_len = 7,
     .field_len = 1,
     .data =
         {
             {.name = "put", .arity = 3, .func = fn_map_put},
         }},
};

/*********************
*  > helpers
********************/

ObjString* resolve_path(VM* vm, ObjString* fname) {
  // import("program.eve"); // relative
  // import("../../program.eve"); // absolute
  // import("Users/you/Documents/foo.eve"); // absolute
  bool is_relative = strchr(fname->str, PATH_SEP) == NULL;
  if (is_relative) {
    // use the current module as base, e.g. /foo/bar/current.eve
    ObjString* base = vm->current_module->name;
    char* last_sep = strrchr(base->str, PATH_SEP);
    if (last_sep != NULL) {
      // concat e.g. foo/bar/ + program.eve
      char buff[PATH_MAX], resolved[PATH_MAX];
      snprintf(
          buff,
          PATH_MAX,
          "%.*s%s",
          (int)(++last_sep - base->str),
          base->str,
          fname->str);
      char* res = NULL;
#ifdef _WIN32
      res = _fullpath(resolved, buff, PATH_MAX);
#else
      res = realpath(buff, resolved);
#endif
      if (res != NULL) {
        return AS_STRING(create_string(
            vm,
            &vm->strings,
            resolved,
            strlen(resolved),
            false));
      }
    }
  } else {
    char resolved[PATH_MAX];
    char* res = NULL;
#ifdef _WIN32
    res = _fullpath(resolved, fname->str, PATH_MAX);
#else
    res = realpath(fname->str, resolved);
#endif
    if (res != NULL) {
      return AS_STRING(create_string(
          vm,
          &vm->strings,
          resolved,
          strlen(resolved),
          false));
    }
  }
  return NULL;
}

void add_cfn(VM* vm, ObjStruct* module, char* name, int arity, CFn func) {
  // adds a CFn object to module
  vm_push_stack(
      vm,
      create_string(vm, &vm->strings, name, (int)strlen(name), false));
  vm_push_stack(vm, OBJ_VAL(create_cfn(vm, func, arity, name)));
  hashmap_put(&module->fields, vm, *(vm->sp - 2), *(vm->sp - 1));
  vm_pop_stack(vm);
  vm_pop_stack(vm);
}

void init_builtins(VM* vm) {
  // setup core module and its members
  ObjString* core = AS_STRING(create_string(
      vm,
      &vm->strings,
      mod_data[0].module_name,
      mod_data[0].name_len,
      false));
  vm->builtins = create_module(vm, core);
  hashmap_init(&vm->builtins->fields);
  for (int i = 0; i < mod_data[0].field_len; i++) {
    struct FnData data = mod_data[0].data[i];
    add_cfn(vm, vm->builtins, data.name, data.arity, data.func);
  }
  // setup other modules
  for (int i = 1; i < sizeof(mod_data) / sizeof(struct ModuleData); i++) {
    struct ModuleData m_data = mod_data[i];
    Value modname_val = create_string(
        vm,
        &vm->strings,
        m_data.module_name,
        m_data.name_len,
        false);
    ObjString* modname = AS_STRING(modname_val);
    ObjStruct* module = create_module(vm, modname);
    hashmap_init(&module->fields);
    for (int j = 0; j < m_data.field_len; j++) {
      struct FnData fn_data = m_data.data[j];
      add_cfn(vm, module, fn_data.name, fn_data.arity, fn_data.func);
    }
    // store the module in the core module
    hashmap_put(&vm->builtins->fields, vm, modname_val, OBJ_VAL(module));
  }
}

void init_module(VM* vm, ObjStruct* module) {
  vm_push_stack(vm, OBJ_VAL(module));
  // store builtins into module's globals as "core"
  hashmap_put(
      &module->fields,
      vm,
      OBJ_VAL(vm->builtins->name),
      OBJ_VAL(vm->builtins));
  vm_pop_stack(vm);
}

inline static bool file_exists(char* path) {
  FILE* file = fopen(path, "r");
  return file && !fclose(file);
}

#define ASSERT_TYPE(vm, check, val, ret, ...) \
  if (!check(val)) { \
    runtime_error(vm, NOTHING_VAL, __VA_ARGS__); \
    return ret; \
  }

/*********************
*  > core
********************/

// print one or more values
Value fn_print(VM* vm, int argc, const Value* args) {
  (void)vm;
  int sh = argc - 1;
  for (int i = 0; i < argc; i++) {
    print_value(*(args + i));
    if (i < sh) {
      fputc(' ', stdout);
    }
  }
  fflush(stdout);
  return NONE_VAL;
}

// print but with newline
Value fn_println(VM* vm, int argc, const Value* args) {
  Value ret = fn_print(vm, argc, args);
  fputc('\n', stdout);
  return ret;
}

// import a module
Value fn_import(VM* vm, int argc, const Value* args) {
  Value value = *args;
  ASSERT_TYPE(
      vm,
      IS_STRING,
      value,
      NONE_VAL,
      "Expected argument of type 'string', but got '%s'",
      get_value_type(value));
  // set flag to prevent the gc from triggering during compilation
  vm->is_compiling = true;
  Value module;
  // if the module is already cached, just return it
  if ((module = hashmap_get(&vm->modules, value)) != NOTHING_VAL) {
    return module;
  } else {
    module = NONE_VAL;
  }
  ObjString* fname = AS_STRING(value);
  ObjString* path = resolve_path(vm, fname);
  if (path == NULL) {
    runtime_error(
        vm,
        NOTHING_VAL,
        "Could not resolve path to module '%s'",
        fname->str);
    return module;
  }
#ifdef EVE_OPTIMIZE_IMPORTS
  char* filename = strrchr(path->str, PATH_SEP);
  char dir_buff[PATH_MAX], eco_file[PATH_MAX];
  if (filename) {
    ++filename;
    char* dir = path->str;
    int dir_len = (int)(filename - path->str);
    snprintf(dir_buff, PATH_MAX, "%.*s__eve__", dir_len, dir);
    char* dot = strrchr(filename, '.');
    ASSERT(dot, "module files should have an extension");
    snprintf(
        eco_file,
        PATH_MAX,
        "%s/%.*s.eve-%d%d%d.eco",
        dir_buff,
        (int)(dot - filename),
        filename,
        EVE_VERSION_MAJOR,
        EVE_VERSION_MINOR,
        EVE_VERSION_PATCH);
    // only use .eco files when the .eve file is available
    if (file_exists(eco_file) && file_exists(path->str)) {
      ObjStruct* current_module = vm->current_module;
      vm->current_module = NULL;  // see $$SERDE_MODULE_HACK$$ in serde.c
      EveSerde serde;
      init_serde(&serde, SD_DESERIALIZE, vm, (error_cb)serde_error_cb);
      ObjFn* func = deserialize(&serde, eco_file);
      free_serde(&serde);
      if (func) {
        ObjClosure* closure = create_closure(vm, func);
        init_module(vm, func->module);
        module = OBJ_VAL(func->module);
        // cache the module
        hashmap_put(&vm->modules, vm, value, module);
        // push a new frame to execute the module
        CallFrame frame = {
            .closure = closure,
            .stack = vm->sp - 1 - argc,
            .ip = func->code.bytes};
        vm_push_frame(vm, frame);
        vm->is_compiling = false;
        return module;
      }
      vm->current_module = current_module;
    }
  }
#endif
  char* src = NULL;
  char* msg = read_file(path->str, &src);
  if (msg != NULL) {
    runtime_error(vm, NOTHING_VAL, "%s - '%s'\n", msg, fname->str);
    src ? free(src) : (void)0;
    return module;
  }
  // parse the module
  Parser parser = new_parser(src, fname->str);
  AstNode* root = parse(&parser);
  if (parser.errors) {
    cleanup_parser(&parser, src);
    vm->has_error = true;
  } else {
    // compile the module if parsing was successful
    Compiler compiler;
    ObjFn* func = create_function(vm);
    ObjClosure* closure = create_closure(vm, func);
    new_compiler(&compiler, root, func, vm, path->str);
    compile(&compiler);
    cleanup_parser(&parser, src);
    if (!compiler.errors) {
      // initialize the module
      init_module(vm, func->module);
      module = OBJ_VAL(func->module);
      // cache the module
      hashmap_put(&vm->modules, vm, value, module);
      // push a new frame to execute the module
      CallFrame frame = {
          .closure = closure,
          .stack = vm->sp - 1 - argc,
          .ip = func->code.bytes};
      vm_push_frame(vm, frame);
#ifdef EVE_OPTIMIZE_IMPORTS
      if (filename) {
        // create the directory if it doesn't exist, or fail silently
        mkdir(dir_buff, 0777);
        // serialize the imported module, and store under __eve__ directory
        EveSerde serde;
        init_serde(&serde, SD_SERIALIZE, vm, (error_cb)serde_error_cb);
        serialize(&serde, eco_file, func);
        free_serde(&serde);
      }
#endif
    } else {
      vm->has_error = true;
    }
  }
  vm->is_compiling = false;
  return module;
}

Value fn_exit(VM* vm, int argc, const Value* args) {
  (void)argc;
  ASSERT_TYPE(
      vm,
      IS_NUMBER,
      *args,
      NONE_VAL,
      "Expected argument of type 'number', but got '%s'",
      get_value_type(*args));
  free_vm(vm);
  exit(AS_NUMBER(*args));
}

Value fn_type(VM* vm, int argc, const Value* args) {
  (void)argc;
  char* type = get_value_type(*args);
  return create_string(vm, &vm->strings, type, (int)strlen(type), false);
}

Value fn_clock(VM* vm, int argc, const Value* args) {
  (void)argc, (void)vm, (void)args;
  return NUMBER_VAL(((double)(clock()) / CLOCKS_PER_SEC));
}

Value fn_time(VM* vm, int argc, const Value* args) {
  (void)argc, (void)vm, (void)args;
  return NUMBER_VAL((double)time(NULL));
}

Value fn_offload(VM* vm, int argc, const Value* args) {
  (void)argc, (void)args;
  hashmap_copy(vm, &vm->current_module->fields, &vm->builtins->fields);
  return NONE_VAL;
}

/***********************
*  > core > string
***********************/
Value fn_str_len(VM* vm, int argc, const Value* args) {
  ASSERT_TYPE(
      vm,
      IS_STRING,
      *args,
      NUMBER_VAL(-1),
      "Expected argument of type 'string', but got '%s'",
      get_value_type(*args));
  (void)argc;
  return NUMBER_VAL(AS_STRING(*args)->length);
}

/**********************
*  > core > list
**********************/
Value fn_list_len(VM* vm, int argc, const Value* args) {
  ASSERT_TYPE(
      vm,
      IS_LIST,
      *args,
      NUMBER_VAL(-1),
      "Expected argument of type 'list', but got '%s'",
      get_value_type(*args));
  (void)argc;
  return NUMBER_VAL(AS_LIST(*args)->elems.length);
}

//Value fn_list_append(VM* vm, int argc, const Value* args) {}

/***********************
*  > core > hashmap
***********************/
Value fn_map_put(VM* vm, int argc, const Value* args) {
  (void)argc;
  Value map = *args, value = *(args + 2);
  ASSERT_TYPE(
      vm,
      IS_HMAP,
      *args,
      NUMBER_VAL(-1),
      "Expected first argument of type 'map', but got '%s'",
      get_value_type(map));
  hashmap_put(AS_HMAP(map), vm, *(args + 1), value);
  return value;
}

/**********************
*  > core > struct
***********************/
//Value fn_struct_fields(VM* vm, int argc, const Value* args) {}

/**********************
*  > core > instance
***********************/
//Value fn_instance_props(VM* vm, int argc, const Value* args) {}

/*********************
*  > core > module
*********************/
//Value fn_module_path(VM* vm, int argc, const Value* args) {}