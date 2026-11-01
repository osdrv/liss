#include "timer.h"

#include <uv.h>

#include "memory.h"
#include "object.h"
#include "vm.h"

typedef struct {
    VM* vm;
    PendingTimer* node;
} AfterCtx;

static void onAfterClosed(uv_handle_t* handle) {
    AfterCtx* ctx = (AfterCtx*)handle->data;
    free(ctx);
    free(handle);
}

static void onAfterFired(uv_timer_t* handle) {
    AfterCtx* ctx = (AfterCtx*)handle->data;
    VM* vm = ctx->vm;

    PendingTimer** link = &vm->pending_timers;
    while (*link != ctx->node) link = &(*link)->next;
    *link = ctx->node->next;

    Value fn = ctx->node->closure;
    reallocate(vm, ctx->node, sizeof(PendingTimer), 0);
    callFromNative(vm, fn, 0, NULL);

    uv_close((uv_handle_t*)handle, onAfterClosed);
}

static void onSleepTimer(uv_timer_t* handle) {
    DEBUG_LOG("timer:sleep -- timer fired");
    uv_close((uv_handle_t*)handle, NULL);
}

static Value afterNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_INT(argv[0])) {
        return raiseErr(vm,
                        "timer:after: expects an integer millisecond count");
    }
    Value fn = argv[1];
    if (!IS_OBJ(fn) ||
        (OBJ_TYPE(fn) != OBJ_CLOSURE && OBJ_TYPE(fn) != OBJ_NATIVE)) {
        return raiseErr(vm, "timer:after: second argument must be a function");
    }
    int64_t ms = AS_INT(argv[0]);
    if (ms < 0) {
        return raiseErr(vm, "timer:after: millisecond count must be >= 0");
    }

    PendingTimer* node = malloc(sizeof(PendingTimer));
    node->closure = fn;
    node->next = vm->pending_timers;
    vm->pending_timers = node;

    AfterCtx* ctx = malloc(sizeof(AfterCtx));
    ctx->vm = vm;
    ctx->node = node;

    uv_timer_t* handle = malloc(sizeof(uv_timer_t));
    uv_timer_init(&vm->loop, handle);
    handle->data = ctx;
    uv_timer_start(handle, onAfterFired, (uint64_t)ms, 0);

    return NIL_VAL;
}

static Value sleepNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_INT(argv[0])) {
        return raiseErr(vm,
                        "timer:sleep: expects an integer millisecond count");
    }
    int64_t ms = AS_INT(argv[0]);
    if (ms < 0) {
        return raiseErr(vm, "timer:sleep: millisecond count must be >= 0");
    }
    uv_timer_t timer;
    uv_timer_init(&vm->loop, &timer);
    uv_timer_start(&timer, onSleepTimer, (uint64_t)ms, 0);

    uv_run(&vm->loop, UV_RUN_DEFAULT);

    return NIL_VAL;
}

static const NativeReg timer_functions[] = {
    {"after", 2, afterNative},
    {"sleep", 1, sleepNative},
    {NULL, 0, NULL},
};

void registerTimerNatives(VM* vm, ObjModule* module) {
    defineNatives(vm, module, timer_functions);
}
