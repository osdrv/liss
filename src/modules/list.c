#include "list.h"

#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "value.h"
#include "vm.h"

static Value headNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_LIST(argv[0])) return raiseErr(vm, "list:head: expects a list");
    ObjList* list = AS_LIST(argv[0]);
    if (list->len == 0) return raiseErr(vm, "list:head: empty list");
    return AS_PAIR(list->head)->first;
}

static Value tailNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_LIST(argv[0])) return raiseErr(vm, "list:tail: expects a list");
    ObjList* list = AS_LIST(argv[0]);
    if (list->len == 0) return raiseErr(vm, "list:tail: empty list");
    Value rest = AS_PAIR(list->head)->second;
    return OBJ_VAL(newList(vm, list->len - 1, rest));
}

static Value lastNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_LIST(argv[0])) return raiseErr(vm, "list:last: expects a list");
    ObjList* list = AS_LIST(argv[0]);
    if (list->len == 0) return raiseErr(vm, "list:last: empty list");
    Value cur = list->head;
    for (uint32_t i = 0; i < list->len - 1; i++) cur = AS_PAIR(cur)->second;
    return AS_PAIR(cur)->first;
}

static Value consNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_LIST(argv[0]))
        return raiseErr(vm, "list:cons: first argument must be a list");
    ObjList* list = AS_LIST(argv[0]);
    push(vm, NIL_VAL);
    vm->stack_top[-1] = OBJ_VAL(newPair(vm, argv[1], list->head));
    Value result = OBJ_VAL(newList(vm, list->len + 1, vm->stack_top[-1]));
    pop(vm);
    return result;
}

// Rebuild the spine of 'list' with 'elem' appended at the end. O(n).
static Value pushNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_LIST(argv[0]))
        return raiseErr(vm, "list:push: first argument must be a list");
    ObjList* list = AS_LIST(argv[0]);
    uint32_t len = list->len;

    Value cur = list->head;
    for (uint32_t i = 0; i < len; i++) {
        push(vm, AS_PAIR(cur)->first);
        cur = AS_PAIR(cur)->second;
    }

    push(vm, NIL_VAL);
    vm->stack_top[-1] = OBJ_VAL(newPair(vm, argv[1], NIL_VAL));

    for (uint32_t i = 0; i < len; i++) {
        Value elem = vm->stack_top[-2];
        vm->stack_top[-1] = OBJ_VAL(newPair(vm, elem, vm->stack_top[-1]));
        vm->stack_top[-2] = vm->stack_top[-1];
        pop(vm);
    }

    Value result = OBJ_VAL(newList(vm, len + 1, vm->stack_top[-1]));
    pop(vm);
    return result;
}

static Value appendNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_LIST(argv[0]) || !IS_LIST(argv[1]))
        return raiseErr(vm, "list:append: expects two lists");
    ObjList* list1 = AS_LIST(argv[0]);
    ObjList* list2 = AS_LIST(argv[1]);

    if (list1->len == 0) return argv[1];
    if (list2->len == 0) return argv[0];

    uint32_t len1 = list1->len;
    uint32_t len2 = list2->len;
    Value list2_head = list2->head;

    Value cur = list1->head;
    for (uint32_t i = 0; i < len1; i++) {
        push(vm, AS_PAIR(cur)->first);
        cur = AS_PAIR(cur)->second;
    }

    // Chain starts at list2's existing spine — structural sharing.
    push(vm, list2_head);

    for (uint32_t i = 0; i < len1; i++) {
        Value elem = vm->stack_top[-2];
        vm->stack_top[-1] = OBJ_VAL(newPair(vm, elem, vm->stack_top[-1]));
        vm->stack_top[-2] = vm->stack_top[-1];
        pop(vm);
    }

    Value result = OBJ_VAL(newList(vm, len1 + len2, vm->stack_top[-1]));
    pop(vm);
    return result;
}

static Value mapNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    Value fn = argv[0];
    if (!IS_OBJ(fn) ||
        (OBJ_TYPE(fn) != OBJ_CLOSURE && OBJ_TYPE(fn) != OBJ_NATIVE))
        return raiseErr(vm, "list:map: first argument must be a function");
    if (!IS_LIST(argv[1]))
        return raiseErr(vm, "list:map: second argument must be a list");

    ObjList* list = AS_LIST(argv[1]);
    uint32_t len = list->len;
    if (len == 0) return argv[1];

    // Collect elements from spine (all rooted through argv[1] on the VM stack).
    Value* elems = malloc(len * sizeof(Value));
    if (elems == NULL) return raiseErr(vm, "list:map: allocation failed");
    Value cur = list->head;
    for (uint32_t i = 0; i < len; i++) {
        elems[i] = AS_PAIR(cur)->first;
        cur = AS_PAIR(cur)->second;
    }

    // Build result chain right-to-left; chain kept rooted at stack_top[-1].
    push(vm, NIL_VAL);
    for (int32_t i = (int32_t)len - 1; i >= 0; i--) {
        Value mapped = callFromNative(vm, fn, 1, &elems[i]);
        if (vm->last_result != INTERPRET_OK) {
            pop(vm);
            free(elems);
            return NIL_VAL;
        }
        push(vm, mapped);
        // stack_top[-1]=mapped, stack_top[-2]=chain
        vm->stack_top[-1] =
            OBJ_VAL(newPair(vm, vm->stack_top[-1], vm->stack_top[-2]));
        vm->stack_top[-2] = vm->stack_top[-1];
        pop(vm);
    }

    Value result = OBJ_VAL(newList(vm, len, vm->stack_top[-1]));
    pop(vm);
    free(elems);
    return result;
}

static Value reduceNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    Value fn = argv[0];
    if (!IS_OBJ(fn) ||
        (OBJ_TYPE(fn) != OBJ_CLOSURE && OBJ_TYPE(fn) != OBJ_NATIVE))
        return raiseErr(vm, "list:reduce: first argument must be a function");
    if (!IS_LIST(argv[2]))
        return raiseErr(vm, "list:reduce: third argument must be a list");

    ObjList* list = AS_LIST(argv[2]);

    // Keep accumulator rooted on the VM stack.
    push(vm, argv[1]);

    Value cur = list->head;
    for (uint32_t i = 0; i < list->len; i++) {
        Value elem = AS_PAIR(cur)->first;
        cur = AS_PAIR(cur)->second;
        Value call_args[2] = {vm->stack_top[-1], elem};
        Value new_acc = callFromNative(vm, fn, 2, call_args);
        if (vm->last_result != INTERPRET_OK) {
            pop(vm);
            return NIL_VAL;
        }
        vm->stack_top[-1] = new_acc;
    }

    Value result = vm->stack_top[-1];
    pop(vm);
    return result;
}

// Three-way comparison for natural order. Sets *type_err on incompatible types.
static int naturalCmp(Value a, Value b, bool* type_err) {
    if (IS_INT(a) && IS_INT(b)) {
        int64_t x = AS_INT(a), y = AS_INT(b);
        return (x > y) - (x < y);
    }
    if (IS_NUMERIC(a) && IS_NUMERIC(b)) {
        double x = IS_INT(a) ? (double)AS_INT(a) : AS_REAL(a);
        double y = IS_INT(b) ? (double)AS_INT(b) : AS_REAL(b);
        return (x > y) - (x < y);
    }
    if (IS_STRING(a) && IS_STRING(b))
        return strcmp(AS_CSTRING(a), AS_CSTRING(b));
    *type_err = true;
    return 0;
}

// Merges elems[lo..mid) and elems[mid..hi) in-place, using tmp[lo..hi) as
// scratch. Returns false (with vm->last_result set) on error.
static bool mergeParts(VM* vm, Value* elems, Value* tmp, uint32_t lo,
                       uint32_t mid, uint32_t hi, Value fn, bool use_fn) {
    memcpy(tmp + lo, elems + lo, (hi - lo) * sizeof(Value));
    uint32_t i = lo, j = mid, k = lo;
    while (i < mid && j < hi) {
        bool a_first;
        if (use_fn) {
            Value args[2] = {tmp[i], tmp[j]};
            Value r = callFromNative(vm, fn, 2, args);
            if (vm->last_result != INTERPRET_OK) return false;
            a_first = IS_BOOL(r) && AS_BOOL(r);
        } else {
            bool type_err = false;
            int cmp = naturalCmp(tmp[i], tmp[j], &type_err);
            if (type_err) {
                (void)raiseErr(
                    vm, "list:sort: cannot compare values of different types");
                return false;
            }
            a_first = (cmp <= 0);
        }
        elems[k++] = a_first ? tmp[i++] : tmp[j++];
    }
    while (i < mid) elems[k++] = tmp[i++];
    while (j < hi) elems[k++] = tmp[j++];
    return true;
}

static bool mergeSort(VM* vm, Value* elems, Value* tmp, uint32_t lo,
                      uint32_t hi, Value fn, bool use_fn) {
    if (hi - lo <= 1) return true;
    uint32_t mid = lo + (hi - lo) / 2;
    return mergeSort(vm, elems, tmp, lo, mid, fn, use_fn) &&
           mergeSort(vm, elems, tmp, mid, hi, fn, use_fn) &&
           mergeParts(vm, elems, tmp, lo, mid, hi, fn, use_fn);
}

static Value sortImpl(VM* vm, Value list_val, Value fn, bool use_fn) {
    ObjList* list = AS_LIST(list_val);
    uint32_t len = list->len;
    if (len <= 1) return list_val;

    Value* elems = malloc(len * sizeof(Value));
    Value* tmp = malloc(len * sizeof(Value));
    if (!elems || !tmp) {
        free(elems);
        free(tmp);
        return raiseErr(vm, "list:sort: allocation failed");
    }

    Value cur = list->head;
    for (uint32_t i = 0; i < len; i++) {
        elems[i] = AS_PAIR(cur)->first;
        cur = AS_PAIR(cur)->second;
    }

    bool ok = mergeSort(vm, elems, tmp, 0, len, fn, use_fn);
    free(tmp);

    if (!ok) {
        free(elems);
        return NIL_VAL;
    }

    // Rebuild chain right-to-left; head kept rooted at stack_top[-1].
    push(vm, NIL_VAL);
    for (int32_t i = (int32_t)len - 1; i >= 0; i--) {
        push(vm, elems[i]);
        vm->stack_top[-1] =
            OBJ_VAL(newPair(vm, vm->stack_top[-1], vm->stack_top[-2]));
        vm->stack_top[-2] = vm->stack_top[-1];
        pop(vm);
    }
    Value result = OBJ_VAL(newList(vm, len, vm->stack_top[-1]));
    pop(vm);
    free(elems);
    return result;
}

static Value sortNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_LIST(argv[0])) return raiseErr(vm, "list:sort: expects a list");
    return sortImpl(vm, argv[0], NIL_VAL, false);
}

static Value sortByNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_LIST(argv[0]))
        return raiseErr(vm, "list:sort_by: first argument must be a list");
    Value fn = argv[1];
    if (!IS_OBJ(fn) ||
        (OBJ_TYPE(fn) != OBJ_CLOSURE && OBJ_TYPE(fn) != OBJ_NATIVE))
        return raiseErr(vm, "list:sort_by: second argument must be a function");
    return sortImpl(vm, argv[0], fn, true);
}

static const NativeReg list_functions[] = {
    {"head", 1, headNative}, {"tail", 1, tailNative},
    {"last", 1, lastNative}, {"cons", 2, consNative},
    {"push", 2, pushNative}, {"append", 2, appendNative},
    {"map", 2, mapNative},   {"reduce", 3, reduceNative},
    {"sort", 1, sortNative}, {"sort_by", 2, sortByNative},
    {NULL, 0, NULL},
};

void registerListNatives(VM* vm, ObjModule* module) {
    defineNatives(vm, module, list_functions);
}
