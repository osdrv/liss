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

static Value raiseNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        // TODO: handle this properly once we have error handling in place
        return NIL_VAL;
    }
    Value error = argv[0];
    if (IS_STRING(error)) {
        ObjError* err = newError(vm, AS_CSTRING(error));
        error = OBJ_VAL(err);
    }
    if (!IS_ERROR(error)) {
        char* str = sprintValue(error);
        error = OBJ_VAL(newError(vm, str));
        free(str);
    }

    vm->raise_value = error;
    vm->last_result = INTERPRET_RUNTIME_ERROR;

    return NIL_VAL;  // This would be ignored
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
    defineNative(vm, "raise!", 1, raiseNative);
}
