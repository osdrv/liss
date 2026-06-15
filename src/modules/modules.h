#ifndef liss_modules_modules_h
#define liss_modules_modules_h

#include "core.h"
#include "io.h"
#include "math.h"
#include "object.h"
#include "re.h"
#include "vm.h"

typedef void (*NativeModuleLoader)(VM* vm, ObjModule* module);

typedef struct {
    const char* name;
    NativeModuleLoader loader;
} NativeModuleEntry;

static const NativeModuleEntry native_module_registry[] = {
    {"core", registerCoreNatives},
    {"math", registerMathNatives},
    {"io", registerIONatives},
    {"re", registerRENatives},
    {NULL, NULL},
};

#endif
