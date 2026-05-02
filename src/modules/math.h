#ifndef liss_modules_math_h
#define liss_modules_math_h

typedef struct VM VM;
typedef struct ObjModule ObjModule;

void registerMathNatives(VM* vm, ObjModule* module);

#endif
