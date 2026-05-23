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
    } else if (IS_DICT(arg)) {
        return INT_VAL((int64_t)AS_DICT(arg)->table.size);
    } else {
        return raiseErr(vm, "len takes a string, list, or dict argument");
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
    } else if (IS_DICT(arg)) {
        return BOOL_VAL(AS_DICT(arg)->table.size == 0);
    } else {
        return raiseErr(vm, "is_empty? takes a string, list, or dict argument");
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

static Value dictNative(VM* vm, int argc, Value* argv) {
    ObjDict* dict = newDict(vm);
    for (int i = 0; i < argc; i++) {
        if (!IS_PAIR(argv[i])) {
            return raiseErr(vm, "dict only accepts a list of pairs");
        }
        ObjPair* pair = AS_PAIR(argv[i]);
        tableInsert(&dict->table, pair->first, pair->second);
    }
    return OBJ_VAL(dict);
}

static Value getNative(VM* vm, int argc, Value* argv) {
    if (argc != 2) {
        return raiseErr(vm, "get expects 2 arguments");
    }
    Value box = argv[0];
    Value key = argv[1];

    if (IS_DICT(box)) {
        Value* val = tableGet(&AS_DICT(box)->table, key);
        return (val != NULL) ? *val : NIL_VAL;
    } else if (IS_LIST(box)) {
        if (!IS_INT(key)) {
            return raiseErr(vm, "list index must be an integer");
        }
        int64_t ix = AS_INT(key);
        ObjList* list = AS_LIST(box);
        if (ix < 0 || ix >= list->len) {
            return raiseErr(vm, "list index out of bounds");
        }
        Value curr = list->head;
        for (int i = 0; i < ix; i++) curr = AS_PAIR(curr)->second;
        return AS_PAIR(curr)->first;
    } else if (IS_STRING(box)) {
        if (!IS_INT(key)) {
            return raiseErr(vm, "string index must be an integer");
        }
        int64_t ix = AS_INT(key);
        ObjString* str = AS_STRING(box);
        if (ix < 0 || ix >= str->length) {
            return raiseErr(vm, "string index out of bounds");
        }
        return OBJ_VAL(copyString(vm, &str->chars[ix], 1));
    }
    return raiseErr(vm, "get argument must be a dict, list or string");
}

static Value putNative(VM* vm, int argc, Value* argv) {
    if (argc != 3) {
        return raiseErr(vm, "put expects 3 arguments");
    }
    if (!IS_DICT(argv[0])) {
        return raiseErr(vm, "put expects dict as the first argument");
    }
    tableInsert(&AS_DICT(argv[0])->table, argv[1], argv[2]);
    return argv[0];
}

static Value hasNative(VM* vm, int argc, Value* argv) {
    if (argc != 2) {
        return raiseErr(vm, "has? expects 2 arguments");
    }
    if (!IS_DICT(argv[0])) {
        return raiseErr(vm, "has? expects a dict as the first argument");
    }
    return BOOL_VAL(tableGet(&AS_DICT(argv[0])->table, argv[1]) != NULL);
}

static Value delNative(VM* vm, int argc, Value* argv) {
    if (argc != 2) {
        return raiseErr(vm, "del expects 2 arguments");
    }
    if (!IS_DICT(argv[0])) {
        return raiseErr(vm, "del expects a dict as the first argument");
    }
    tableRemove(&AS_DICT(argv[0])->table, argv[1]);
    return NIL_VAL;
}

static Value keysNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raiseErr(vm, "keys expects 1 argument");
    }
    if (!IS_DICT(argv[0])) {
        return raiseErr(vm, "keys expects a dict as the first argument");
    }
    ObjDict* dict = AS_DICT(argv[0]);
    Value head = NIL_VAL;
    push(vm, head);
    Table* table = &dict->table;
    for (size_t i = 0; i < table->bucket_count; i++) {
        TableEntry* entry = table->buckets[i];
        while (entry != NULL) {
            head = OBJ_VAL(newPair(vm, entry->key, head));
            vm->stack_top[-1] = head;
            entry = entry->next;
        }
    }
    Value result = OBJ_VAL(newList(vm, dict->table.size, head));
    pop(vm);
    return result;
}

static Value valuesNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raiseErr(vm, "values expects 1 argument");
    }
    if (!IS_DICT(argv[0])) {
        return raiseErr(vm, "values expects a dict as the first argument");
    }
    ObjDict* dict = AS_DICT(argv[0]);
    Value head = NIL_VAL;
    push(vm, head);
    Table* table = &dict->table;
    for (size_t i = 0; i < table->bucket_count; i++) {
        TableEntry* entry = table->buckets[i];
        while (entry != NULL) {
            head = OBJ_VAL(newPair(vm, entry->value, head));
            vm->stack_top[-1] = head;
            entry = entry->next;
        }
    }
    Value result = OBJ_VAL(newList(vm, dict->table.size, head));
    pop(vm);
    return result;
}

static const NativeReg core_functions[] = {
    {"err!", 1, errNative},          {"is_err?", 1, isErrNative},
    {"raise!", 1, raiseNative},      {"len", 1, lenNative},
    {"is_empty?", 1, isEmptyNative}, {"pair", 2, pairNative},
    {"dict", -1, dictNative},        {"get", 2, getNative},
    {"put", 3, putNative},           {"has?", 2, hasNative},
    {"del", 2, delNative},           {"keys", 1, keysNative},
    {"values", 1, valuesNative},     {NULL, 0, NULL},  // Sentinel value
};

void registerCoreNatives(VM* vm, ObjModule* module) {
    defineNatives(vm, module, core_functions);
}
