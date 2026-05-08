#include "core.h"

#include "object.h"
#include "value.h"
#include "vm.h"

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
        return raiseErr(vm, "len takes exactly 1 argument");
    }
    Value arg = argv[0];
    if (IS_STRING(arg)) {
        return INT_VAL(AS_STRING(arg)->length);
    } else if (IS_LIST(arg)) {
        return INT_VAL(AS_LIST(arg)->len);
    } else {
        return raiseErr(vm, "len takes a string or list argument");
    }
}

static Value isEmptyNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raiseErr(vm, "is_empty? takes exactly 1 argument");
    }
    Value arg = argv[0];
    if (IS_STRING(arg)) {
        return BOOL_VAL(AS_STRING(arg)->length == 0);
    } else if (IS_LIST(arg)) {
        return BOOL_VAL(AS_LIST(arg)->len == 0);
    } else {
        return raiseErr(vm, "is_empty? takes a string or list argument");
    }
}

static Value pairNative(VM* vm, int argc, Value* argv) {
    if (argc != 2) {
        return raiseErr(vm, "pair takes exactly 2 arguments");
    }
    Value first = argv[0];
    Value second = argv[1];
    return OBJ_VAL(newPair(vm, first, second));
}

static const NativeReg core_functions[] = {
    {"err!", 1, errNative},
    {"is_err?", 1, isErrNative},
    {"raise!", 1, raiseNative},
    {"len", 1, lenNative},
    {"is_empty?", 1, isEmptyNative},
    {"pair", 2, pairNative},
    {NULL, 0, NULL},  // Sentinel value
};

void registerCoreNatives(VM* vm, ObjModule* module) {
    defineNatives(vm, module, core_functions);
}
