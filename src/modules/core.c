#include "core.h"

#include <stdio.h>
#include <string.h>

#include "hamt.h"
#include "object.h"
#include "value.h"
#include "vm.h"

static Value errNative(VM* vm, int argc, Value* argv) {
    (void)argc;
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
    (void)argc;
    (void)vm;
    return BOOL_VAL(IS_ERROR(argv[0]));
}

static Value raiseNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_ERROR(argv[0])) {
        return raiseErr(vm, "raise! expects an err value");
    }
    vm->raise_value = argv[0];
    vm->last_result = INTERPRET_RUNTIME_ERROR;
    return NIL_VAL;
}

static Value noErrNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_ERROR(argv[0])) return argv[0];
    vm->raise_value = argv[0];
    vm->last_result = INTERPRET_RUNTIME_ERROR;
    return NIL_VAL;
}

static Value lenNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    Value arg = argv[0];
    if (IS_STRING(arg)) {
        return INT_VAL(AS_STRING(arg)->length);
    } else if (IS_LIST(arg)) {
        return INT_VAL(AS_LIST(arg)->len);
    } else if (IS_DICT(arg)) {
        return INT_VAL((int64_t)AS_DICT(arg)->count);
    }

    return raiseErr(vm, "len takes a string, list, or dict argument");
}

static Value isEmptyNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    Value arg = argv[0];
    if (IS_STRING(arg)) {
        return BOOL_VAL(AS_STRING(arg)->length == 0);
    } else if (IS_LIST(arg)) {
        return BOOL_VAL(AS_LIST(arg)->len == 0);
    } else if (IS_DICT(arg)) {
        return BOOL_VAL(AS_DICT(arg)->count == 0);
    }

    return raiseErr(vm, "is_empty? takes a string, list, or dict argument");
}

static Value pairNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    return OBJ_VAL(newPair(vm, argv[0], argv[1]));
}

static Value fstNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_PAIR(argv[0])) return raiseErr(vm, "fst expects a pair");
    return AS_PAIR(argv[0])->first;
}

static Value sndNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_PAIR(argv[0])) return raiseErr(vm, "snd expects a pair");
    return AS_PAIR(argv[0])->second;
}

static Value dictNative(VM* vm, int argc, Value* argv) {
    ObjDict* dict = newDict(vm);
    push(vm, OBJ_VAL(dict));
    for (int i = 0; i < argc; i++) {
        if (!IS_PAIR(argv[i])) {
            pop(vm);
            return raiseErr(vm, "dict only accepts a list of pairs");
        }
        ObjPair* pair = AS_PAIR(argv[i]);
        uint64_t hash = hamtHash(pair->first);
        bool is_new = hamtGet(dict->root, pair->first, hash, 0) == NULL;
        dict->root =
            hamtPut(vm, dict->root, pair->first, pair->second, hash, 0);
        if (is_new) dict->count++;
    }
    pop(vm);
    return OBJ_VAL(dict);
}

static Value getNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    Value box = argv[0];
    Value key = argv[1];

    if (IS_DICT(box)) {
        Value* val = hamtGet(AS_DICT(box)->root, key, hamtHash(key), 0);
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
    (void)argc;
    if (!IS_DICT(argv[0])) {
        return raiseErr(vm, "put expects dict as the first argument");
    }
    ObjDict* old = AS_DICT(argv[0]);
    uint64_t hash = hamtHash(argv[1]);
    bool is_new = hamtGet(old->root, argv[1], hash, 0) == NULL;
    HamtNode* new_root = hamtPut(vm, old->root, argv[1], argv[2], hash, 0);
    push(vm, OBJ_VAL((Obj*)new_root));
    ObjDict* d = newDict(vm);
    d->root = (HamtNode*)AS_OBJ(pop(vm));
    d->count = old->count + (is_new ? 1 : 0);

    return OBJ_VAL(d);
}

static Value hasNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_DICT(argv[0])) {
        return raiseErr(vm, "has? expects a dict as the first argument");
    }

    ObjDict* dict = AS_DICT(argv[0]);
    return BOOL_VAL(hamtGet(dict->root, argv[1], hamtHash(argv[1]), 0) != NULL);
}

static Value delNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_DICT(argv[0])) {
        return raiseErr(vm, "del expects a dict as the first argument");
    }
    ObjDict* old = AS_DICT(argv[0]);
    uint64_t hash = hamtHash(argv[1]);
    bool existed = hamtGet(old->root, argv[1], hash, 0) != NULL;
    HamtNode* new_root = hamtDel(vm, old->root, argv[1], hash, 0);
    push(vm, OBJ_VAL((Obj*)new_root));
    ObjDict* d = newDict(vm);
    d->root = (HamtNode*)AS_OBJ(pop(vm));
    d->count = old->count - (existed ? 1 : 0);
    return OBJ_VAL(d);
}

static void keyCb(Value key, Value val, void* ctx) {
    (void)val;
    VM* vm = (VM*)ctx;
    vm->stack_top[-1] = OBJ_VAL(newPair(vm, key, vm->stack_top[-1]));
}

static void valCb(Value key, Value val, void* ctx) {
    (void)key;
    VM* vm = (VM*)ctx;
    vm->stack_top[-1] = OBJ_VAL(newPair(vm, val, vm->stack_top[-1]));
}

static Value keysNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_DICT(argv[0])) {
        return raiseErr(vm, "keys expects a dict as the first argument");
    }
    ObjDict* dict = AS_DICT(argv[0]);
    push(vm, NIL_VAL);
    hamtEach(dict->root, keyCb, vm);
    Value result = OBJ_VAL(newList(vm, dict->count, vm->stack_top[-1]));
    pop(vm);
    return result;
}

static Value valuesNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_DICT(argv[0])) {
        return raiseErr(vm, "values expects a dict as the first argument");
    }
    ObjDict* dict = AS_DICT(argv[0]);
    push(vm, NIL_VAL);
    hamtEach(dict->root, valCb, vm);
    Value result = OBJ_VAL(newList(vm, dict->count, vm->stack_top[-1]));
    pop(vm);
    return result;
}

static Value strNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (IS_STRING(argv[0])) return argv[0];  // already a string
    char* s = sprintValue(argv[0]);
    Value result = OBJ_VAL(copyString(vm, s, strlen(s)));
    free(s);
    return result;
}

static Value toIntNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (IS_INT(argv[0])) return argv[0];
    if (IS_REAL(argv[0])) return INT_VAL((int64_t)AS_REAL(argv[0]));
    return raiseErr(vm, "to_int: expected int or real");
}

static Value toRealNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (IS_REAL(argv[0])) return argv[0];
    if (IS_INT(argv[0])) return REAL_VAL((double)AS_INT(argv[0]));
    return raiseErr(vm, "to_real: expected int or real");
}

static const char* valTypeName(Value v) {
    switch (v.type) {
        case VAL_INT:  return "int";
        case VAL_REAL: return "real";
        case VAL_BOOL: return "bool";
        case VAL_NIL:  return "nil";
        case VAL_OBJ:
            switch (OBJ_TYPE(v)) {
                case OBJ_STRING:   return "string";
                case OBJ_LIST:     return "list";
                case OBJ_PAIR:     return "pair";
                case OBJ_DICT:     return "dict";
                case OBJ_CLOSURE:
                case OBJ_FUNCTION: return "fn";
                case OBJ_NATIVE:   return "native-fn";
                case OBJ_ERROR:    return "error";
                case OBJ_RE:       return "re";
                case OBJ_MODULE:   return "module";
                case OBJ_FILE:     return "file";
                default:           return "obj";
            }
        default: return "?";
    }
}

static Value inspectNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    Value v = argv[0];

    if (IS_NIL(v))
        return OBJ_VAL(copyString(vm, "nil", 3));

    const char* type = valTypeName(v);
    char* buf;
    int len;

    if (IS_CLOSURE(v)) {
        // sprintValue returns "<object>" for closures — use the name directly.
        ObjString* name = AS_CLOSURE(v)->function->name;
        const char* n = name ? name->chars : "(anon)";
        len = snprintf(NULL, 0, "%s: %s", type, n);
        buf = malloc(len + 1);
        snprintf(buf, len + 1, "%s: %s", type, n);
    } else if (IS_RE(v)) {
        // sprintValue returns "<object>" for regex — show the pattern.
        const char* pat = AS_RE(v)->pattern->chars;
        len = snprintf(NULL, 0, "%s: /%s/", type, pat);
        buf = malloc(len + 1);
        snprintf(buf, len + 1, "%s: /%s/", type, pat);
    } else if (IS_ERROR(v)) {
        // sprintValue wraps in "<error: ...>" — show the message directly.
        const char* msg = AS_ERROR(v)->message->chars;
        len = snprintf(NULL, 0, "%s: %s", type, msg);
        buf = malloc(len + 1);
        snprintf(buf, len + 1, "%s: %s", type, msg);
    } else {
        char* repr = sprintValue(v);
        len = snprintf(NULL, 0, "%s: %s", type, repr);
        buf = malloc(len + 1);
        snprintf(buf, len + 1, "%s: %s", type, repr);
        free(repr);
    }

    Value result = OBJ_VAL(copyString(vm, buf, len));
    free(buf);
    return result;
}

static const NativeReg core_functions[] = {
    {"err", 1, errNative},      {"is_err?", 1, isErrNative},
    {"raise!", 1, raiseNative}, {"noerr!", 1, noErrNative},
    {"len", 1, lenNative},      {"is_empty?", 1, isEmptyNative},
    {"pair", 2, pairNative},    {"fst", 1, fstNative},
    {"snd", 1, sndNative},      {"dict", -1, dictNative},
    {"get", 2, getNative},      {"put", 3, putNative},
    {"has?", 2, hasNative},     {"del", 2, delNative},
    {"keys", 1, keysNative},    {"values", 1, valuesNative},
    {"str", 1, strNative},      {"to_int", 1, toIntNative},
    {"to_real", 1, toRealNative}, {"inspect", 1, inspectNative},
    {NULL, 0, NULL},  // Sentinel value
};

void registerCoreNatives(VM* vm, ObjModule* module) {
    defineNatives(vm, module, core_functions);
}
