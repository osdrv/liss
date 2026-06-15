#ifndef liss_modules_re
#define liss_modules_re

#include "object.h"

typedef struct VM VM;

void registerRENatives(VM* vm, ObjModule* module);

#endif
