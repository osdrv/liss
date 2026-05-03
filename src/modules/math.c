#include <math.h>

#include "object.h"
#include "vm.h"

/**
 * Returns the largest integer value less than or equal to the argument.
 *
 * Arguments: 1
 * Argument types: Int or Real
 * Return type: Real
 */
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

/**
 * Returns the smallest integer value greater than or equal to the argument.
 *
 * Arguments: 1
 * Argument types: Int or Real
 * Return type: Real
 */
static Value ceilNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raiseErr(vm, "ceil takes exactly 1 argument");
    }
    Value arg = argv[0];
    if (!(IS_INT(arg) || IS_REAL(arg))) {
        return raiseErr(vm, "ceil takes int or real argument");
    }
    double val = (IS_INT(arg) ? (double)AS_INT(arg) : AS_REAL(arg));
    double res = ceil(val);
    return REAL_VAL(res);
}

/**
 * Returns the value rounded to the nearest integer.
 *
 * Arguments: 1
 * Argument types: Int or Real
 * Return type: Real
 */
static Value roundNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raiseErr(vm, "round takes exactly 1 argument");
    }
    Value arg = argv[0];
    if (!(IS_INT(arg) || IS_REAL(arg))) {
        return raiseErr(vm, "round takes int or real argument");
    }
    double val = (IS_INT(arg) ? (double)AS_INT(arg) : AS_REAL(arg));
    double res = round(val);
    return REAL_VAL(res);
}

/**
 * Returns the absolute value of the argument.
 *
 * Arguments: 1
 * Argument types: Int or Real
 * Return type: Real
 */
static Value absNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raiseErr(vm, "abs takes exactly 1 argument");
    }
    Value arg = argv[0];
    if (!(IS_INT(arg) || IS_REAL(arg))) {
        return raiseErr(vm, "abs takes int or real argument");
    }
    double val = (IS_INT(arg) ? (double)AS_INT(arg) : AS_REAL(arg));
    double res = fabs(val);
    return REAL_VAL(res);
}

/**
 * Returns the square root of the argument.
 *
 * Arguments: 1
 * Argument types: Int or Real (must be non-negative)
 * Return type: Real
 */
static Value sqrtNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raiseErr(vm, "sqrt takes exactly 1 argument");
    }
    Value arg = argv[0];
    if (!(IS_INT(arg) || IS_REAL(arg))) {
        return raiseErr(vm, "sqrt takes int or real argument");
    }
    double val = (IS_INT(arg) ? (double)AS_INT(arg) : AS_REAL(arg));
    if (val < 0) {
        return raiseErr(vm, "sqrt of negative number is not defined");
    }
    double res = sqrt(val);
    return REAL_VAL(res);
}

/**
 * Returns the base raised to the power of the exponent.
 *
 * Arguments: 2
 * Argument types: [Base: Int or Real, Exponent: Int or Real]
 * Return type: Real
 */
static Value powNative(VM* vm, int argc, Value* argv) {
    if (argc != 2) {
        return raiseErr(vm, "pow takes exactly 2 arguments");
    }
    Value base = argv[0];
    Value exp = argv[1];
    if (!(IS_INT(base) || IS_REAL(base) || IS_INT(exp) || IS_REAL(exp))) {
        return raiseErr(vm, "pow takes int or real arguments");
    }
    double baseVal = (IS_INT(base) ? (double)AS_INT(base) : AS_REAL(base));
    double expVal = (IS_INT(exp) ? (double)AS_INT(exp) : AS_REAL(exp));
    double res = pow(baseVal, expVal);
    return REAL_VAL(res);
}

/**
 * Returns the remainder of the floating-point division of the first argument by
 * the second.
 *
 * Arguments: 2
 * Argument types: [Dividend: Int or Real, Divisor: Int or Real]
 * Return type: Real
 */
static Value fmodNative(VM* vm, int argc, Value* argv) {
    if (argc != 2) {
        return raiseErr(vm, "fmod takes exactly 2 arguments");
    }
    Value arg1 = argv[0];
    Value arg2 = argv[1];
    if (!(IS_INT(arg1) || IS_REAL(arg1) || IS_INT(arg2) || IS_REAL(arg2))) {
        return raiseErr(vm, "fmod takes int or real arguments");
    }
    double val1 = (IS_INT(arg1) ? (double)AS_INT(arg1) : AS_REAL(arg1));
    double val2 = (IS_INT(arg2) ? (double)AS_INT(arg2) : AS_REAL(arg2));
    double res = fmod(val1, val2);
    return REAL_VAL(res);
}

/**
 * Returns the natural logarithm (base-e) of the argument.
 *
 * Arguments: 1
 * Argument types: Int or Real (must be greater than zero)
 * Return type: Real
 */
static Value logNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raiseErr(vm, "log takes exactly 1 argument");
    }
    Value arg = argv[0];
    if (!(IS_INT(arg) || IS_REAL(arg))) {
        return raiseErr(vm, "log takes int or real arguments");
    }
    double val = (IS_INT(arg) ? (double)AS_INT(arg) : AS_REAL(arg));
    if (val <= 0) {
        return raiseErr(vm, "log argument must be greater than zero");
    }
    double res = log(val);
    return REAL_VAL(res);
}

/**
 * Returns the base-2 logarithm of the argument.
 *
 * Arguments: 1
 * Argument types: Int or Real (must be greater than zero)
 * Return type: Real
 */
static Value log2Native(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raiseErr(vm, "log2 takes exactly 1 argument");
    }
    Value arg = argv[0];
    if (!(IS_INT(arg) || IS_REAL(arg))) {
        return raiseErr(vm, "log2 takes int or real arguments");
    }
    double val = (IS_INT(arg) ? (double)AS_INT(arg) : AS_REAL(arg));
    if (val <= 0) {
        return raiseErr(vm, "log2 argument must be greater than zero");
    }
    double res = log2(val);
    return REAL_VAL(res);
}

/**
 * Returns the value of e (Euler's number) raised to the power of the argument.
 *
 * Arguments: 1
 * Argument types: Int or Real
 * Return type: Real
 */
static Value expNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raiseErr(vm, "exp takes exactly 1 argument");
    }
    Value arg = argv[0];
    if (!(IS_INT(arg) || IS_REAL(arg))) {
        return raiseErr(vm, "exp takes int or real arguments");
    }
    double val = (IS_INT(arg) ? (double)AS_INT(arg) : AS_REAL(arg));
    double res = exp(val);
    return REAL_VAL(res);
}

/**
 * Returns the sine of the argument (expected in radians).
 *
 * Arguments: 1
 * Argument types: Int or Real
 * Return type: Real
 */
static Value sinNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raiseErr(vm, "sin takes exactly 1 argument");
    }
    Value arg = argv[0];
    if (!(IS_INT(arg) || IS_REAL(arg))) {
        return raiseErr(vm, "sin takes int or real arguments");
    }
    double val = (IS_INT(arg) ? (double)AS_INT(arg) : AS_REAL(arg));
    double res = sin(val);
    return REAL_VAL(res);
}

/**
 * Returns the cosine of the argument (expected in radians).
 *
 * Arguments: 1
 * Argument types: Int or Real
 * Return type: Real
 */
static Value cosNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raiseErr(vm, "cos takes exactly 1 argument");
    }
    Value arg = argv[0];
    if (!(IS_INT(arg) || IS_REAL(arg))) {
        return raiseErr(vm, "cos takes int or real arguments");
    }
    double val = (IS_INT(arg) ? (double)AS_INT(arg) : AS_REAL(arg));
    double res = cos(val);
    return REAL_VAL(res);
}

/**
 * Returns the tangent of the argument (expected in radians).
 *
 * Arguments: 1
 * Argument types: Int or Real
 * Return type: Real
 */
static Value tanNative(VM* vm, int argc, Value* argv) {
    if (argc != 1) {
        return raiseErr(vm, "tan takes exactly 1 argument");
    }
    Value arg = argv[0];
    if (!(IS_INT(arg) || IS_REAL(arg))) {
        return raiseErr(vm, "tan takes int or real arguments");
    }
    double val = (IS_INT(arg) ? (double)AS_INT(arg) : AS_REAL(arg));
    double res = tan(val);
    return REAL_VAL(res);
}

/**
 * Returns the arc tangent of y/x, using the signs of the arguments to determine
 * the quadrant.
 *
 * Arguments: 2
 * Argument types: [y: Int or Real, x: Int or Real]
 * Return type: Real
 */
static Value atan2Native(VM* vm, int argc, Value* argv) {
    if (argc != 2) {
        return raiseErr(vm, "atan2 takes exactly 2 arguments");
    }
    Value arg1 = argv[0];
    Value arg2 = argv[1];
    if (!(IS_INT(arg1) || IS_REAL(arg1)) || !(IS_INT(arg2) || IS_REAL(arg2))) {
        return raiseErr(vm, "atan2 takes int or real arguments");
    }
    double val1 = (IS_INT(arg1) ? (double)AS_INT(arg1) : AS_REAL(arg1));
    double val2 = (IS_INT(arg2) ? (double)AS_INT(arg2) : AS_REAL(arg2));
    double res = atan2(val1, val2);
    return REAL_VAL(res);
}

static const NativeReg math_functions[] = {
    {"floor", 1, floorNative}, {"ceil", 1, ceilNative},
    {"round", 1, roundNative}, {"abs", 1, absNative},
    {"sqrt", 1, sqrtNative},   {"pow", 2, powNative},
    {"fmod", 2, fmodNative},   {"log", 1, logNative},
    {"log2", 1, log2Native},   {"exp", 1, expNative},
    {"sin", 1, sinNative},     {"cos", 1, cosNative},
    {"tan", 1, tanNative},     {"atan2", 2, atan2Native},
    {NULL, 0, NULL},  // Sentinel value
};

void registerMathNatives(VM* vm, ObjModule* module) {
    defineNatives(vm, module, math_functions);
};
