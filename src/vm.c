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

Value peek(VM* vm) { return *(vm->stack_top - 1); }

// --- VM Execution (Direct Threading) ---

static InterpretResult run(VM* vm) {
#define BINARY_OP(op)                                       \
    do {                                                    \
        Value b = pop(vm);                                  \
        Value a = pop(vm);                                  \
        push(vm, NUMBER_VAL(AS_NUMBER(a) op AS_NUMBER(b))); \
    } while (false)

#define COMPARISON_OP(op)                                         \
    do {                                                          \
        Value b = pop(vm);                                        \
        Value a = pop(vm);                                        \
        if (a.type != b.type) {                                   \
            fprintf(stderr, "Comparison type mismatch error.\n"); \
            return INTERPRET_RUNTIME_ERROR;                       \
        }                                                         \
        push(vm, BOOL_VAL(AS_NUMBER(a) op AS_NUMBER(b)));         \
    } while (false)

#define READ_BYTE() (*ip++)
#define READ_SHORT() (uint16_t)((*ip++ << 8) | (*ip++))

#if defined(__GNUC__) || defined(__clang__)
    // --- Direct Threading Setup ---
    // The dispatch table: an array of opcode implementation addresses.
    static void* dispatch_table[] = {
        &&OP_RETURN_IMPL,   &&OP_CONSTANT_IMPL,      &&OP_POP_IMPL,
        &&OP_JUMP_IMPL,     &&OP_JUMP_IF_FALSE_IMPL, &&OP_ADD_IMPL,
        &&OP_SUBTRACT_IMPL, &&OP_MULTIPLY_IMPL,      &&OP_DIVIDE_IMPL,
        &&OP_TRUE_IMPL,     &&OP_FALSE_IMPL,         &&OP_NULL_IMPL,
        &&OP_NOT_IMPL,      &&OP_EQUAL_IMPL,         &&OP_GREATER_IMPL,
        &&OP_LESS_IMPL,
    };

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
            case OP_JUMP:
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = (uint16_t)(bytecode[0] << 8) | bytecode[1];
                bytecode += 2;
                loaded_code[loaded_idx++] = (void*)(uintptr_t)offset;
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
    vm->last_popped_value = pop(vm);
    free(loaded_code);  // Clean up the loaded code
    return INTERPRET_OK;
}

OP_CONSTANT_IMPL: {
    // The operand is the next pointer in the instruction stream.
    Value* const_ix = (Value*)*ip++;
    push(vm, *const_ix);
    DISPATCH();
}

OP_POP_IMPL: {
    pop(vm);
    DISPATCH();
}

OP_JUMP_IMPL: {
    uint16_t offset = (uint16_t)(uintptr_t)(*ip++);
    ip += offset;
    DISPATCH();
}

OP_JUMP_IF_FALSE_IMPL: {
    uint16_t offset = (uint16_t)(uintptr_t)(*ip++);
    if (isFalsey(peek(vm))) {
        ip += offset;
    }
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

OP_TRUE_IMPL: {
    push(vm, BOOL_VAL(true));
    DISPATCH();
}

OP_FALSE_IMPL: {
    push(vm, BOOL_VAL(false));
    DISPATCH();
}
OP_NULL_IMPL: {
    push(vm, NIL_VAL);
    DISPATCH();
}

OP_NOT_IMPL: {
    Value value = pop(vm);
    push(vm, BOOL_VAL(isFalsey(value)));
    DISPATCH();
}

OP_EQUAL_IMPL: {
    Value b = pop(vm);
    Value a = pop(vm);
    push(vm, BOOL_VAL(valuesEqual(a, b)));
    DISPATCH();
}

OP_GREATER_IMPL: {
    COMPARISON_OP(>);
    DISPATCH();
}

OP_LESS_IMPL: {
    COMPARISON_OP(<);
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
