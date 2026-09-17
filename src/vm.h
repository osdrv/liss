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
    size_t young_threshold;  // trigger minor GC when new_bytes exceeds this
    bool stress_gc;  // If true, trigger GC on every allocation (for testing)
    bool stress_minor_gc;  // trigger minor GC on every allocation (for testing)
} VMOptions;

typedef struct VM {
    VMOptions options;
    size_t bytes_allocated;
    size_t next_gc;
    size_t new_bytes;  // bytes allocated in the new generation since the last
                       // minor GC

    Obj** rmb_set;
    size_t rmb_cnt;
    size_t rmb_cap;

    CallFrame* frames;
    int frame_cnt;
    int frame_cap;

    Value* stack_top;
    InterpretResult last_result;  // Store the last interpret result

    Obj* old_objs;
    Obj* new_objs;
    Table strings;
    Table modules;
    ObjModule* core_module;  // The core module containing built-in functions
                             // and constants
    ObjModule* main_module;

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
        .gc_threshold = 1024 * 1024,   // 1MB
        .young_threshold = 256 * 1024,  // 256KB
        .heap_growth_factor = 2,
        .stack_capacity = 256,
        .stress_gc = false,
        .stress_minor_gc = false,
    };
    return options;
}

// Creates and initializes a new VM with a given stack capacity.
VM* newVM(VMOptions options);

// Destroys the VM and frees all associated memory.
void destroyVM(VM* vm);

void vmRecover(VM* vm);

ObjModule* loadModule(VM* vm, ObjString* module_name);

// The main entry point for running source code.
InterpretResult interpret(VM* vm, const char* source, ObjModule* module);

// Stack operations
void push(VM* vm, Value value);
Value pop(VM* vm);
Value peek(VM* vm, int distance);

// Call a Liss closure or native from a C native function.
Value callFromNative(VM* vm, Value callee, int argc, Value* argv);

void printStack(VM* vm);
void printConsts(Chunk* chunk);

#endif
