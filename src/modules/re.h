#ifndef liss_modules_re_h
#define liss_modules_re_h

#include "object.h"

typedef struct VM VM;

void registerRENatives(VM* vm, ObjModule* module);

#endif
