#include "vm.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "gc.h"
#include "memory.h"
#include "object.h"
#include "opcode.h"
#include "table.h"
#include "value.h"

// --- Forward Declarations ---
static InterpretResult run(VM* vm);
static void** loadThreadedCode(Chunk* chunk, void* dispatch_table[]);

// --- VM Lifecycle ---

VM* newVM(size_t stack_capacity) {
    VM* vm =
        (VM*)reallocate(NULL, 0, sizeof(VM) + sizeof(Value) * stack_capacity);
    vm->stack_capacity = stack_capacity;
    vm->stack_top = vm->stack;
    vm->objects = NULL;
    vm->last_result = INTERPRET_OK;
    initTable(&vm->strings);
    initTable(&vm->globals);
    DEBUG_LOG("Initialized new VM with stack capacity %zu.", stack_capacity);
    return vm;
}

void destroyVM(VM* vm) {
    if (vm == NULL) return;
    freeTable(&vm->strings);
    freeTable(&vm->globals);
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

    // Keep the function object alive by pushing it onto the stack
    push(vm, OBJ_VAL(function));

    vm->frame_count = 1;
    CallFrame* frame = &vm->frames[0];
    frame->function = function;
    frame->slots = vm->stack;
    frame->ip = NULL;
    frame->code = NULL;
    frame->slots = NULL;

    InterpretResult result = run(vm);
    return result;
}

// --- Stack Operations ---

void push(VM* vm, Value value) {
    if ((size_t)(vm->stack_top - vm->stack) >= vm->stack_capacity) {
        fprintf(stderr, "Stack overflow.\n");
        vm->last_result = INTERPRET_RUNTIME_ERROR;
        return;
    }
    *vm->stack_top = value;
    vm->stack_top++;
}

Value pop(VM* vm) {
    if (vm->stack_top == vm->stack) {
        fprintf(stderr, "Stack underflow.\n");
        vm->last_result = INTERPRET_RUNTIME_ERROR;
        return NIL_VAL;
    }
    vm->stack_top--;
    return *vm->stack_top;
}

Value peek(VM* vm, int distance) { return vm->stack_top[-1 - distance]; }

typedef bool (*BinaryOpFn)(VM* vm, Value a, Value b);

static bool addNumbers(VM* vm, Value a, Value b) {
    pop(vm);
    pop(vm);
    push(vm, NUMBER_VAL(AS_NUMBER(a) + AS_NUMBER(b)));
    return true;
}

static bool concatStrings(VM* vm, Value a, Value b) {
    ObjString* left = AS_STRING(a);
    ObjString* right = AS_STRING(b);

    int length = left->length + right->length;
    char* chars = (char*)malloc(length + 1);
    if (chars == NULL) {
        fprintf(stderr, "Memory allocation failed for string concatenation.\n");
        return false;
    }
    memcpy(chars, left->chars, left->length);
    memcpy(chars + left->length, right->chars, right->length);
    chars[length] = '\0';

    ObjString* result = copyString(vm, chars, length);
    free(chars);

    pop(vm);
    pop(vm);

    push(vm, OBJ_VAL(result));
    return true;
}

static bool multiplyNumbers(VM* vm, Value a, Value b) {
    pop(vm);
    pop(vm);
    push(vm, NUMBER_VAL(AS_NUMBER(a) * AS_NUMBER(b)));
    return true;
}

static bool duplicateString(VM* vm, Value a, Value b) {
    if (!IS_STRING(a) || !IS_NUMBER(b)) {
        fprintf(stderr,
                "Type error: Expected string and number for duplication.\n");
        return false;
    }

    ObjString* str = AS_STRING(a);
    double count_double = AS_NUMBER(b);

    if (count_double < 0 || trunc(count_double) != count_double) {
        fprintf(
            stderr,
            "Value error: Duplication count must be a non-negative integer.\n");
        return false;
    }

    int count = (int)count_double;

    if (count == 0) {
        pop(vm);
        pop(vm);
        push(vm, OBJ_VAL(copyString(vm, "", 0)));
        return true;
    }

    if (count == 1) {
        pop(vm);
        return true;  // No duplication needed
    }

    int len = str->length * count;
    char* chars = (char*)malloc(len + 1);
    if (chars == NULL) {
        fprintf(stderr, "Memory allocation failed for string duplication.\n");
        return false;
    }

    char* pos = chars;
    for (int i = 0; i < count; i++) {
        memcpy(pos, str->chars, str->length);
        pos += str->length;
    }
    chars[len] = '\0';
    ObjString* res = takeString(vm, chars, len);

    pop(vm);
    pop(vm);
    push(vm, OBJ_VAL(res));
    return true;
}

// TODO: uncomment if needed or remove
// static void printStack(VM* vm) {
//     printf("VM stack:\n");
//     for (Value* slot = vm->stack; slot < vm->stack_top; slot++) {
//         printf("  [ ");
//         printValue(*slot);
//         printf(" ]\n");
//     }
//     printf("\n");
// }

// TODO: uncomment if needed or remove
// static void printConsts(VM* vm) {
//     printf("Constants:\n");
//     for (int i = 0; i < vm->chunk->constants.count; i++) {
//         printf("  [%d] ", i);
//         printValue(vm->chunk->constants.values[i]);
//         printf("\n");
//     }
//     printf("\n");
// }

// --- VM Execution (Direct Threading) ---

static void** loadThreadedCode(Chunk* chunk, void* dispatch_table[]) {
    InterpretResult result = INTERPRET_OK;
    // "Loader" stage: Convert byte-based chunk to pointer-based code.
    size_t loaded_code_size = sizeof(void*) * chunk->count;
    void** loaded_code = (void**)malloc(loaded_code_size);
    if (!loaded_code) {
        fprintf(stderr, "Memory error loading threaded code.\n");
        result = INTERPRET_RUNTIME_ERROR;
        goto LOADER_CLEANUP;
    }

    int* byte_to_slot_map = malloc(sizeof(int) * chunk->count);
    if (!byte_to_slot_map) {
        fprintf(stderr, "Memory error allocating byte-to-slot map.\n");
        result = INTERPRET_RUNTIME_ERROR;
        goto LOADER_CLEANUP;
    }
    for (int i = 0; i < chunk->count; i++) {
        byte_to_slot_map[i] = -1;
    }

    int* jumps_to_patch = NULL;
    int jump_count = 0;
    int jumps_capacity = 0;

    uint8_t* bytecode = chunk->code;
    int loaded_idx = 0;

    DEBUG_LOG("Loader first pass: Translating bytecode to threaded code.");
    while (bytecode < chunk->code + chunk->count) {
        int byte_offset = bytecode - chunk->code;
        byte_to_slot_map[byte_offset] = loaded_idx;

        uint8_t opcode = *bytecode++;
        loaded_code[loaded_idx++] = dispatch_table[opcode];

        switch (opcode) {
            case OP_CONSTANT: {
                uint16_t const_index =
                    (uint16_t)(bytecode[0] << 8) | bytecode[1];
                bytecode += 2;
                loaded_code[loaded_idx++] =
                    (void*)&chunk->constants.values[const_index];
                break;
            }
            case OP_JUMP:
            case OP_JUMP_IF_FALSE: {
                // Read the relative offset from the original bytecode
                uint16_t relative_byte_offset =
                    (uint16_t)(bytecode[0] << 8) | bytecode[1];
                // The offset is relative to the byte after the jump operands
                int target_byte_addr =
                    (bytecode - chunk->code) + 2 + relative_byte_offset;

                loaded_code[loaded_idx] = (void*)(uintptr_t)target_byte_addr;
                if (jumps_capacity < jump_count + 1) {
                    int old_capacity = jumps_capacity;
                    jumps_capacity =
                        jumps_capacity < 8 ? 8 : jumps_capacity * 2;
                    jumps_to_patch = (int*)reallocate(
                        jumps_to_patch, sizeof(int) * old_capacity,
                        sizeof(int) * jumps_capacity);
                    if (jumps_to_patch == NULL) {
                        fprintf(stderr,
                                "Memory error allocating jumps to patch.\n");
                        result = INTERPRET_RUNTIME_ERROR;
                        goto LOADER_CLEANUP;
                    }
                }
                jumps_to_patch[jump_count++] = loaded_idx;
                bytecode += 2;
                loaded_idx++;
                break;
            }
            case OP_GET_GLOBAL:
            case OP_SET_GLOBAL: {
                uint16_t const_index =
                    (uint16_t)(bytecode[0] << 8) | bytecode[1];
                bytecode += 2;
                loaded_code[loaded_idx++] = (void*)(uintptr_t)const_index;
                break;
            }
            default:
                break;  // No operands
        }
    }
    loaded_code = reallocate(loaded_code, sizeof(void*) * chunk->count,
                             sizeof(void*) * loaded_idx);
    if (loaded_code == NULL) {
        fprintf(stderr, "Memory error resizing loaded code.\n");
        result = INTERPRET_RUNTIME_ERROR;
        goto LOADER_CLEANUP;
    }

    DEBUG_LOG("Loader second pass: Patching jump offsets.");
    for (int i = 0; i < jump_count; i++) {
        int operand_slot_ix = jumps_to_patch[i];
        int target_byte_addr = (int)(uintptr_t)loaded_code[operand_slot_ix];
        int target_slot_ix = byte_to_slot_map[target_byte_addr];
        if (target_slot_ix == -1) {
            fprintf(stderr, "Invalid jump target during patching.\n");
            result = INTERPRET_RUNTIME_ERROR;
            goto LOADER_CLEANUP;
        }

        int relative_slot_offset = target_slot_ix - (operand_slot_ix + 1);
        loaded_code[operand_slot_ix] = (void*)(uintptr_t)relative_slot_offset;
    }

LOADER_CLEANUP:
    if (byte_to_slot_map != NULL) {
        free(byte_to_slot_map);
    }
    reallocate(jumps_to_patch, sizeof(int) * jumps_capacity, 0);
    if (result != INTERPRET_OK) {
        reallocate(loaded_code, sizeof(void*) * loaded_idx, 0);
        return NULL;
    }
    return loaded_code;
}

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
            result = INTERPRET_RUNTIME_ERROR;                     \
            goto RETURN;                                          \
        }                                                         \
        push(vm, BOOL_VAL(AS_NUMBER(a) op AS_NUMBER(b)));         \
    } while (false)

#define READ_BYTE() (*ip++)
#define READ_SHORT() (uint16_t)((*ip++ << 8) | (*ip++))

    // The dispatch table: an array of opcode implementation addresses.
    static void* dispatch_table[] = {
        &&OP_RETURN_IMPL,     &&OP_CONSTANT_IMPL,      &&OP_POP_IMPL,
        &&OP_JUMP_IMPL,       &&OP_JUMP_IF_FALSE_IMPL, &&OP_ADD_IMPL,
        &&OP_SUBTRACT_IMPL,   &&OP_MULTIPLY_IMPL,      &&OP_DIVIDE_IMPL,
        &&OP_NEGATE_IMPL,     &&OP_TRUE_IMPL,          &&OP_FALSE_IMPL,
        &&OP_NULL_IMPL,       &&OP_NOT_IMPL,           &&OP_EQUAL_IMPL,
        &&OP_GREATER_IMPL,    &&OP_LESS_IMPL,          &&OP_SET_GLOBAL_IMPL,
        &&OP_GET_GLOBAL_IMPL,
    };

    InterpretResult result = INTERPRET_OK;
    CallFrame* frame = &vm->frames[vm->frame_count - 1];
    if (frame->ip == NULL) {
        frame->code = loadThreadedCode(&frame->function->chunk, dispatch_table);
        if (frame->code == NULL) {
            result = INTERPRET_RUNTIME_ERROR;
            goto RETURN;
        }
        frame->ip = frame->code;
    }

#if defined(__GNUC__) || defined(__clang__)

#define DISPATCH()                             \
    do {                                       \
        if (vm->last_result != INTERPRET_OK) { \
            result = vm->last_result;          \
            goto RETURN;                       \
        }                                      \
        goto*(*frame->ip++);                   \
    } while (0)

    // --- Start Execution ---
    DEBUG_LOG("Starting direct-threaded execution.");
    DISPATCH();

#define READ_CONSTANT() ((Value*)*frame->ip++)
#define READ_ARG() ((uintptr_t)*frame->ip++)

    DISPATCH();

    // --- Opcode Implementations ---

OP_RETURN_IMPL: {
    Value res = pop(vm);
    vm->last_popped_value = res;

    free(frame->code);
    frame->code = NULL;
    frame->ip = NULL;

    vm->frame_count--;
    if (vm->frame_count == 0) {
        pop(vm);  // Pop the initial function object to allow GC
        goto RETURN;
    }
    vm->stack_top = frame->slots;
    push(vm, res);

    frame = &vm->frames[vm->frame_count - 1];
    DISPATCH();
}

OP_CALL_IMPL: {
    int arg_count = (int)READ_ARG();
    Value callee = peek(vm, arg_count);

    if (!IS_OBJ(callee) || OBJ_TYPE(callee) != OBJ_FUNCTION) {
        fprintf(stderr, "Runtime error: Can only call functions.\n");
        result = INTERPRET_RUNTIME_ERROR;
        goto RETURN;
    }

    ObjFunction* function = AS_FUNCTION(callee);
    if (arg_count != function->arity) {
        fprintf(
            stderr,
            "Function %s: Runtime error: Expected %d arguments but got %d.\n",
            function->name, function->arity, arg_count);
        result = INTERPRET_RUNTIME_ERROR;
        goto RETURN;
    }

    if (vm->frame_count == FRAMES_MAX) {
        fprintf(stderr,
                "Runtime error: Stack overflow (too many call frames).\n");
        result = INTERPRET_RUNTIME_ERROR;
        goto RETURN;
    }

    frame = &vm->frames[vm->frame_count++];
    frame->function = function;
    frame->slots = vm->stack_top - arg_count - 1;
    frame->ip = loadThreadedCode(&function->chunk, dispatch_table);
    DISPATCH();
}

OP_CONSTANT_IMPL: {
    Value* const_ix = READ_CONSTANT();
    push(vm, *const_ix);
    DISPATCH();
}

OP_POP_IMPL: {
    pop(vm);
    DISPATCH();
}

OP_JUMP_IMPL: {
    uint16_t offset = (uint16_t)(uintptr_t)(*frame->ip++);
    frame->ip += offset;
    DISPATCH();
}

OP_JUMP_IF_FALSE_IMPL: {
    uint16_t offset = (uint16_t)(uintptr_t)(*frame->ip++);
    if (isFalsey(peek(vm, 0))) {
        frame->ip += offset;
    }
    DISPATCH();
}

OP_ADD_IMPL: {
    Value b = peek(vm, 0);
    Value a = peek(vm, 1);
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        if (!addNumbers(vm, a, b)) {
            result = INTERPRET_RUNTIME_ERROR;
            goto RETURN;
        }
    } else if (IS_STRING(a) && IS_STRING(b)) {
        if (!concatStrings(vm, a, b)) {
            result = INTERPRET_RUNTIME_ERROR;
            goto RETURN;
        }
    } else {
        fprintf(stderr,
                "Runtime error: Operands must be two numbers or two strings "
                "for addition.\n");
        result = INTERPRET_RUNTIME_ERROR;
        goto RETURN;
    }
    DISPATCH();
}

OP_SUBTRACT_IMPL: {
    BINARY_OP(-);
    DISPATCH();
}

OP_MULTIPLY_IMPL: {
    Value b = peek(vm, 0);
    Value a = peek(vm, 1);

    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        if (!multiplyNumbers(vm, a, b)) {
            result = INTERPRET_RUNTIME_ERROR;
            goto RETURN;
        }
    } else if (IS_STRING(a) && IS_NUMBER(b)) {
        if (!duplicateString(vm, a, b)) {
            result = INTERPRET_RUNTIME_ERROR;
            goto RETURN;
        }
    } else {
        fprintf(stderr,
                "Runtime error: Operands must be two numbers or a string and "
                "a number for multiplication.\n");
        result = INTERPRET_RUNTIME_ERROR;
        goto RETURN;
    }
    DISPATCH();
}

OP_DIVIDE_IMPL: {
    BINARY_OP(/);
    DISPATCH();
}

OP_NEGATE_IMPL: {
    Value value = pop(vm);
    if (!IS_NUMBER(value)) {
        fprintf(stderr,
                "Runtime error: Operand must be a number for negation.\n");
        result = INTERPRET_RUNTIME_ERROR;
        goto RETURN;
    }
    push(vm, NUMBER_VAL(-AS_NUMBER(value)));
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

OP_SET_GLOBAL_IMPL: {
    uint16_t const_ix = (uint16_t)READ_ARG();
    Value name = frame->function->chunk.constants.values[const_ix];
    tableInsert(&vm->globals, name, peek(vm, 0));
    DISPATCH();
}

OP_GET_GLOBAL_IMPL: {
    uint16_t const_ix = (uint16_t)READ_ARG();
    Value name = frame->function->chunk.constants.values[const_ix];
    Value* value = tableGet(&vm->globals, name);
    if (value == NULL) {
        fprintf(stderr, "Undefined variable '%.*s'.\n", AS_STRING(name)->length,
                AS_STRING(name)->chars);
        result = INTERPRET_RUNTIME_ERROR;
        goto RETURN;
    }
    push(vm, *value);
    DISPATCH();
}

RETURN:
    if (frame->code != NULL) {
        free(frame->code);
        frame->code = NULL;
        frame->ip = NULL;
    }
    return result;

#else
// Fallback for compilers that don't support computed gotos (e.g., MSVC)
#warning "Computed gotos not supported, falling back to switch-based dispatch."
    // ... switch-based implementation would go here ...
    fprintf(stderr, "Direct threading not supported by this compiler.\n");
    return INTERPRET_RUNTIME_ERROR;
#endif
}
