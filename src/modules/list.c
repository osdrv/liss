#include "list.h"

#include <stdlib.h>

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

static const NativeReg list_functions[] = {
    {"head", 1, headNative}, {"tail", 1, tailNative},
    {"last", 1, lastNative}, {"cons", 2, consNative},
    {"push", 2, pushNative}, {"append", 2, appendNative},
    {"map", 2, mapNative},   {"reduce", 3, reduceNative},
    {NULL, 0, NULL},
};

void registerListNatives(VM* vm, ObjModule* module) {
    defineNatives(vm, module, list_functions);
}
