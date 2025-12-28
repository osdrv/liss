#ifndef liss_compiler_h
#define liss_compiler_h

#include "vm.h"

ObjFunction* compile(VM* vm, const char* source);

#endif
