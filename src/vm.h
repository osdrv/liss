#ifndef liss_vm_h
#define liss_vm_h

#include "common.h"

typedef struct {
    // VM state will go here
} VM;

void initVM(VM* vm);
void freeVM(VM* vm);

#endif
