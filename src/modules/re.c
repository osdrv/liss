#include "re.h"

#include "object.h"
#include "regex.h"
#include "vm.h"

static Value reNative(VM* vm, int argc, Value* argv) {
    if (argc != 1 || !IS_STRING(argv[0])) {
        return raiseErr(vm, "re:re expects a pattern string");
    }
    ObjString* pattern = AS_STRING(argv[0]);
    char* postfix = re2postfix(pattern->chars);
    if (!postfix) return raiseErr(vm, "Invalid regex pattern");

    ReProgram* prog = compileRegex(postfix);
    free(postfix);

    ObjRe* re_obj = newRe(vm, pattern);
    re_obj->program = prog;

    return OBJ_VAL(re_obj);
}

static Value matchQuestNative(VM* vm, int argc, Value* argv) {
    if (argc != 2 || !IS_RE(argv[0]) || !IS_STRING(argv[1])) {
        return raiseErr(vm, "re:match? expects a regex object and a string");
    }

    ObjRe* re_obj = AS_RE(argv[0]);
    const char* text = AS_CSTRING(argv[1]);

    bool result = match((ReProgram*)re_obj->program, text);
    return BOOL_VAL(result);
}

static const NativeReg re_functions[] = {
    {"re", 1, reNative},
    {"match?", 2, matchQuestNative},
    {NULL, 0, NULL},
};

void registerRENatives(VM* vm, ObjModule* module) {
    defineNatives(vm, module, re_functions);
}
