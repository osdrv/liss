#ifndef liss_vm_h
#define liss_vm_h

#include "chunk.h"  // Include for Chunk definition
#include "common.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define STACK_MAX 256
#define TRY_MAX 64
#define MAX_MODULES 256
#define MAX_MODULE_SYMBOLS \
    128  // We need to limit this to avoid module table rehashing

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct {
    ObjClosure* closure;
    void** ip;
    Value* slots;
} CallFrame;

typedef struct {
    void** handler_ip;  // Instruction pointer to jump to on exception
    int frame_cnt;      // How many frames were active when the try block was
                        // entered
    Value* stack_top;   // Stack top at the time of entering the try block
} TryBlock;

typedef struct {
    size_t stack_capacity;
    size_t gc_threshold;
    size_t heap_growth_factor;
    size_t frames_max;
    bool stress_gc;  // If true, trigger GC on every allocation (for testing)
} VMOptions;

typedef struct VM {
    VMOptions options;
    size_t bytes_allocated;
    size_t next_gc;

    CallFrame* frames;
    int frame_cnt;
    int frame_cap;

    Value* stack_top;
    InterpretResult last_result;  // Store the last interpret result

    Obj* objects;  // Linked list of all heap-allocated objects for GC
    Table strings;
    Table modules;
    ObjModule* core_module;  // The core module containing built-in functions
                             // and constants

    Value last_popped_value;    // Store the last popped value
    ObjUpvalue* open_upvalues;  // Linked list of open upvalues

    void* compiler;  // Current compiler (if any) to help GC mark its roots

    TryBlock try_stack[TRY_MAX];
    int try_cnt;
    Value raise_value;
    char error_msg[512];

    // (!!!) Flexible Array Member for the stack. Keep at the end.
    Value stack[];
} VM;

static inline VMOptions defaultVMOptions() {
    VMOptions options = {
        .frames_max = 32,
        .gc_threshold = 1024 * 1024,  // 1MB
        .heap_growth_factor = 2,
        .stack_capacity = 256,
        .stress_gc = false,
    };
    return options;
}

// Creates and initializes a new VM with a given stack capacity.
VM* newVM(VMOptions options);

// Destroys the VM and frees all associated memory.
void destroyVM(VM* vm);

ObjModule* loadModule(VM* vm, ObjString* module_name);

// The main entry point for running source code.
InterpretResult interpret(VM* vm, const char* source, ObjModule* module);

// Stack operations
void push(VM* vm, Value value);
Value pop(VM* vm);
Value peek(VM* vm, int distance);

void printStack(VM* vm);
void printConsts(Chunk* chunk);

#endif
