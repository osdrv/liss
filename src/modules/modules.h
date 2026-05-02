#ifndef liss_modules_modules_h
#define liss_modules_modules_h

#include "math.h"
#include "object.h"
#include "vm.h"

typedef void (*NativeModuleLoader)(VM* vm, ObjModule* module);

typedef struct {
    const char* name;
    NativeModuleLoader loader;
} NativeModuleEntry;

static const NativeModuleEntry native_module_registry[] = {
    {"math", registerMathNatives},
    {NULL, NULL},
};

#endif
