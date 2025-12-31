#include "vm.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "gc.h"
#include "memory.h"
#include "object.h"
#include "opcode.h"
#include "value.h"

// --- Forward Declarations ---
static InterpretResult run(VM* vm);

// --- VM Lifecycle ---

VM* newVM(size_t stack_capacity) {
    VM* vm =
        (VM*)reallocate(NULL, 0, sizeof(VM) + sizeof(Value) * stack_capacity);
    vm->stack_capacity = stack_capacity;
    vm->stack_top = vm->stack;
    vm->objects = NULL;
    // The 'chunk' will be set by the compiler/loader before running.
    vm->chunk = NULL;
    DEBUG_LOG("Initialized new VM with stack capacity %zu.", stack_capacity);
    return vm;
}

void destroyVM(VM* vm) {
    DEBUG_LOG("Destroying VM.");
    Obj* object = vm->objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
    // Correctly free the VM struct and its flexible array member
    reallocate(vm, sizeof(VM) + sizeof(Value) * vm->stack_capacity, 0);
}

// --- Public API ---

InterpretResult interpret(VM* vm, const char* source) {
    ObjFunction* function = compile(vm, source);
    if (function == NULL) {
        return INTERPRET_COMPILE_ERROR;
    }
    vm->chunk = &function->chunk;

    InterpretResult result = run(vm);
    return result;
}

// --- Stack Operations ---

void push(VM* vm, Value value) {
    if ((size_t)(vm->stack_top - vm->stack) >= vm->stack_capacity) {
        fprintf(stderr, "Stack overflow.\n");
        exit(1);
    }
    *vm->stack_top = value;
    vm->stack_top++;
}

Value pop(VM* vm) {
    if (vm->stack_top == vm->stack) {
        fprintf(stderr, "Stack underflow.\n");
        exit(1);
    }
    vm->stack_top--;
    return *vm->stack_top;
}

// --- Value Printing ---

void printValue(Value value) {
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_NUMBER:
            printf("Number: %g", AS_NUMBER(value));
            break;
        case VAL_OBJ:
            switch (OBJ_TYPE(value)) {
                case OBJ_STRING:
                    printf("\"%s\"", AS_CSTRING(value));
                    break;
                case OBJ_FUNCTION:
                    printf("<fn %s>", AS_FUNCTION(value)->name
                                          ? AS_FUNCTION(value)->name->chars
                                          : "script");
                    break;
            }
            break;
    }
}

// --- VM Execution (Direct Threading) ---

static InterpretResult run(VM* vm) {
#define BINARY_OP(op)                                       \
    do {                                                    \
        Value b = pop(vm);                                  \
        Value a = pop(vm);                                  \
        push(vm, NUMBER_VAL(AS_NUMBER(a) op AS_NUMBER(b))); \
    } while (false)

#if defined(__GNUC__) || defined(__clang__)
    // --- Direct Threading Setup ---
    // The dispatch table: an array of opcode implementation addresses.
    static void* dispatch_table[] = {&&OP_RETURN_IMPL,   &&OP_CONSTANT_IMPL,
                                     &&OP_ADD_IMPL,      &&OP_SUBTRACT_IMPL,
                                     &&OP_MULTIPLY_IMPL, &&OP_DIVIDE_IMPL};

    // The instruction pointer for direct threading is a pointer to a pointer.
    void** ip = NULL;

    // "Loader" stage: Convert byte-based chunk to pointer-based code.
    // This is a simplified loader. A real one would be more robust.
    // The size is an over-estimation, but safe for this demo.
    size_t loaded_code_size = sizeof(void*) * vm->chunk->count;
    void** loaded_code = (void**)malloc(loaded_code_size);
    if (!loaded_code) {
        fprintf(stderr, "Memory error loading threaded code.\n");
        return INTERPRET_RUNTIME_ERROR;
    }

    uint8_t* bytecode = vm->chunk->code;
    int loaded_idx = 0;
    while (bytecode < vm->chunk->code + vm->chunk->count) {
        uint8_t opcode = *bytecode++;
        loaded_code[loaded_idx++] = dispatch_table[opcode];
        switch (opcode) {
            case OP_CONSTANT: {
                uint8_t const_index = *bytecode++;
                // In direct threading, operands follow the instruction pointer.
                loaded_code[loaded_idx++] =
                    (void*)&vm->chunk->constants.values[const_index];
                break;
            }
            case OP_RETURN:
                break;  // No operands
        }
    }

    ip = loaded_code;

#define DISPATCH() goto** ip++

    // --- Start Execution ---
    DEBUG_LOG("Starting direct-threaded execution.");
    DISPATCH();

    // --- Opcode Implementations ---

OP_RETURN_IMPL: {
    vm->last_value = pop(vm);
    free(loaded_code);  // Clean up the loaded code
    return INTERPRET_OK;
}

OP_CONSTANT_IMPL: {
    // The operand is the next pointer in the instruction stream.
    Value* const_ix = (Value*)*ip++;
    push(vm, *const_ix);
    DISPATCH();
}

OP_ADD_IMPL: {
    BINARY_OP(+);
    DISPATCH();
}
OP_SUBTRACT_IMPL: {
    BINARY_OP(-);
    DISPATCH();
}
OP_MULTIPLY_IMPL: {
    BINARY_OP(*);
    DISPATCH();
}
OP_DIVIDE_IMPL: {
    BINARY_OP(/);
    DISPATCH();
}

#else
// Fallback for compilers that don't support computed gotos (e.g., MSVC)
#warning "Computed gotos not supported, falling back to switch-based dispatch."
    // ... switch-based implementation would go here ...
    fprintf(stderr, "Direct threading not supported by this compiler.\n");
    return INTERPRET_RUNTIME_ERROR;
#endif
}
