#ifndef liss_vm_h
#define liss_vm_h

#include "chunk.h"  // Include for Chunk definition
#include "common.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define STACK_MAX 256
#define FRAMES_MAX 16  // TODO: make this configurable

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct {
    ObjFunction* function;
    void** ip;
    Value* slots;
} CallFrame;

typedef struct VM {
    CallFrame frames[FRAMES_MAX];
    int frame_count;

    size_t stack_capacity;
    Value* stack_top;
    InterpretResult last_result;  // Store the last interpret result

    Obj* objects;  // Linked list of all heap-allocated objects for GC
    Table strings;
    Table globals;

    Value last_popped_value;  // Store the last popped value

    // (!!!) Flexible Array Member for the stack. Keep at the end.
    Value stack[];
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
Value peek(VM* vm, int distance);

void printStack(VM* vm);
void printConsts(Chunk* chunk);

#endif
