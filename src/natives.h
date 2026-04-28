#ifndef liss_natives_h
#define liss_natives_h

#include "table.h"
#include "value.h"

// Forward declaration of VM to avoid circular dependency
typedef struct VM VM;
typedef struct ObjModule ObjModule;

// The signature for all native functions
typedef Value (*NativeFn)(VM* vm, int arg_count, Value* args);

// Registers a native function with the VM
void defineNative(VM* vm, Table* registry, const char* name, int arity,
                  NativeFn function);

// Registers all core native functions with the VM
void registerCoreNatives(VM* vm, ObjModule* core_module);

#endif
