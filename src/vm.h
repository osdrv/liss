#ifndef liss_vm_h
#define liss_vm_h

#include "common.h"
#include "value.h"
#include "object.h"

#define STACK_MAX 256

typedef struct VM {
    size_t stack_capacity;
    Value* stack_top;
    
    // Linked list of all heap-allocated objects for GC
    Obj* objects;

    // The stack is a flexible array member, allocated at runtime.
    Value stack[];
} VM;

// Creates and initializes a new VM with a given stack capacity.
VM* newVM(size_t stack_capacity);

// Destroys the VM and frees all associated memory.
void destroyVM(VM* vm);

// Stack operations
void push(VM* vm, Value value);
Value pop(VM* vm);

#endif
