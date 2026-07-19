#include "re.h"

#include "object.h"
#include "regex.h"
#include "vm.h"

static Value reNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_STRING(argv[0])) {
        return raiseErr(vm, "re:re expects a pattern string");
    }
    ObjString* pattern = AS_STRING(argv[0]);
    ReProgram* prog = compilePattern(pattern->chars);
    if (!prog) return raiseErr(vm, "Invalid regex pattern");

    ObjRe* re_obj = newRe(vm, pattern);
    re_obj->program = prog;

    return OBJ_VAL(re_obj);
}

static Value matchQuestNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_RE(argv[0]) || !IS_STRING(argv[1])) {
        return raiseErr(vm, "re:match? expects a regex object and a string");
    }

    ObjRe* re_obj = AS_RE(argv[0]);
    const char* text = AS_CSTRING(argv[1]);

    bool result = match((ReProgram*)re_obj->program, text);
    return BOOL_VAL(result);
}

static Value matchNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_RE(argv[0]) || !IS_STRING(argv[1])) {
        return raiseErr(vm, "re:match expects a regex object and a string");
    }

    ObjRe* re_obj = AS_RE(argv[0]);
    const char* text = AS_CSTRING(argv[1]);
    ReProgram* prog = (ReProgram*)re_obj->program;

    const char* submatch[MAX_GROUPS * 2];
    bool result = matchGroups(prog, text, submatch);
    if (!result) {
        return NIL_VAL;  // TODO: shall we return an empty array instead?
    }

    Value head = NIL_VAL;
    push(vm, head);
    int count = prog->num_grps;

    for (int i = count - 1; i >= 0; i--) {
        Value group_val = NIL_VAL;
        if (submatch[2 * i] != NULL && submatch[2 * i + 1] != NULL) {
            int len = submatch[2 * i + 1] - submatch[2 * i];
            ObjString* str = copyString(vm, submatch[2 * i], len);
            group_val = OBJ_VAL(str);
        }
        push(vm, group_val);
        ObjPair* pair = newPair(vm, group_val, head);
        pop(vm);

        head = OBJ_VAL(pair);
        *(vm->stack_top - 1) = head;
    }

    DEBUG_LOG("REGEX group count: %d", count);

    ObjList* list = newList(vm, count, head);
    pop(vm);
    return OBJ_VAL(list);
}

static const NativeReg re_functions[] = {
    {"re", 1, reNative},
    {"match?", 2, matchQuestNative},
    {"match", 2, matchNative},
    {NULL, 0, NULL},
};

void registerRENatives(VM* vm, ObjModule* module) {
    defineNatives(vm, module, re_functions);
}
