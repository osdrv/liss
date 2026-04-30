#include "natives.h"

#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "value.h"
#include "vm.h"

// Internal helper to create an error and set it as the current raise value
static Value raise(VM* vm, const char* message) {
    ObjError* error = newError(vm, message);
    vm->raise_value = OBJ_VAL(error);
    vm->last_result = INTERPRET_RUNTIME_ERROR;
    return NIL_VAL;
}

static Value errNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        // TODO: raise! once we've implemented it
        return NIL_VAL;
    }
    ObjError* error;
    // If it's already a string, we can create the error directly from it.
    if (IS_STRING(argv[0])) {
        error = newError(vm, AS_CSTRING(argv[0]));
    } else {
        char* str = sprintValue(argv[0]);
        error = newError(vm, str);
        free(str);
    }

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

static Value lenNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raise(vm, "len expects exactly 1 argument");
    }
    Value arg = argv[0];
    if (IS_STRING(arg)) {
        return INT_VAL(AS_STRING(arg)->length);
    } else if (IS_LIST(arg)) {
        return INT_VAL(AS_LIST(arg)->len);
    } else {
        return raise(vm, "len expects a string or list argument");
    }
}

static Value isEmptyNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raise(vm, "is_empty? expects exactly 1 argument");
    }
    Value arg = argv[0];
    if (IS_STRING(arg)) {
        return BOOL_VAL(AS_STRING(arg)->length == 0);
    } else if (IS_LIST(arg)) {
        return BOOL_VAL(AS_LIST(arg)->len == 0);
    } else {
        return raise(vm, "is_empty? expects a string or list argument");
    }
}

void defineNative(VM* vm, ObjModule* module, const char* name, int arity,
                  NativeFn function) {
    ObjString* str = copyString(vm, name, (int)strlen(name));
    push(vm, OBJ_VAL(str));
    ObjNative* native = newNative(vm, name, arity, function);
    push(vm, OBJ_VAL(native));
    tableInsert(&module->symbols, OBJ_VAL(str), OBJ_VAL(native));
    pop(vm);
    pop(vm);
}

void registerCoreNatives(VM* vm, ObjModule* module) {
    defineNative(vm, module, "err!", 1, errNative);
    defineNative(vm, module, "is_err?", 1, isErrNative);
    defineNative(vm, module, "raise!", 1, raiseNative);
    defineNative(vm, module, "len", 1, lenNative);
    defineNative(vm, module, "is_empty?", 1, isEmptyNative);
}
