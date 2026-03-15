#include "natives.h"

#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "value.h"
#include "vm.h"

static Value errNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        // TODO: raise! once we've implemented it
        return NIL_VAL;
    }
    char* str = sprintValue(argv[0]);
    ObjError* error = newError(vm, str);
    free(str);

    return OBJ_VAL(error);
}

static Value isErrNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) return BOOL_VAL(false);
    return BOOL_VAL(IS_ERROR(argv[0]));
}

void defineNative(VM* vm, const char* name, int arity, NativeFn function) {
    ObjString* str = copyString(vm, name, (int)strlen(name));
    push(vm, OBJ_VAL(str));
    ObjNative* native = newNative(vm, name, arity, function);
    push(vm, OBJ_VAL(native));
    tableInsert(&vm->globals, OBJ_VAL(str), OBJ_VAL(native));
    pop(vm);
    pop(vm);
}

void registerCoreNatives(VM* vm) {
    defineNative(vm, "err!", 1, errNative);
    defineNative(vm, "is_err?", 1, isErrNative);
}
