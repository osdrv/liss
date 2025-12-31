#ifndef liss_vm_h
#define liss_vm_h

#include "chunk.h"  // Include for Chunk definition
#include "common.h"
#include "object.h"
#include "value.h"

#define STACK_MAX 256

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct VM {
    Chunk* chunk;  // The chunk of code this VM is executing
    size_t stack_capacity;
    Value* stack_top;

    Obj* objects;  // Linked list of all heap-allocated objects for GC

    Value last_value;  // Store the last popped value

    Value stack[];  // Flexible Array Member for the stack
} VM;

// Creates and initializes a new VM with a given stack capacity.
VM* newVM(size_t stack_capacity);

// Destroys the VM and frees all associated memory.
void destroyVM(VM* vm);

// The main entry point for running source code.
InterpretResult interpret(VM* vm, const char* source);

// Stack operations
void push(VM* vm, Value value);
Value pop(VM* vm);

#endif
