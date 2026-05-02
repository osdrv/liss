#include <math.h>

#include "natives.h"
#include "object.h"
#include "vm.h"

static Value floorNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raiseErr(vm, "floor takes exactly 1 argument");
    }
    Value arg = argv[0];
    if (!(IS_INT(arg) || IS_REAL(arg))) {
        return raiseErr(vm, "floor takes int or real argument");
    }
    double val = (IS_INT(arg) ? (double)AS_INT(arg) : AS_REAL(arg));
    double res = floor(val);
    return REAL_VAL(res);
}

void registerMathNatives(VM* vm, ObjModule* module) {
    defineNative(vm, module, "floor", 1, floorNative);
    // TODO: add more functions
}
