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
static int loadThreadedCode(ObjFunction* function, void* dispatch_table[]);

// --- VM Lifecycle ---

VM* newVM(size_t stack_capacity) {
    VM* vm =
        (VM*)reallocate(NULL, 0, sizeof(VM) + sizeof(Value) * stack_capacity);
    vm->stack_capacity = stack_capacity;
    vm->stack_top = vm->stack;
    vm->objects = NULL;
    vm->last_result = INTERPRET_OK;
    vm->open_upvalues = NULL;
    initTable(&vm->strings);
    initTable(&vm->globals);
    DEBUG_LOG("Initialized new VM with stack capacity %zu", stack_capacity);
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
    push(vm, OBJ_VAL(function));  // Push the function for GC safety
    ObjClosure* closure = newClosure(vm, function);
    pop(vm);  // Pop the function after creating the closure

    vm->stack_top = vm->stack;  // Reset stack top for new execution
    push(vm, OBJ_VAL(closure));

    vm->frame_count = 1;
    CallFrame* frame = &vm->frames[0];
    frame->closure = closure;
    frame->slots = vm->stack;
    frame->ip = NULL;

    InterpretResult result = run(vm);
    return result;
}

// --- Stack Operations ---

void push(VM* vm, Value value) {
    if ((size_t)(vm->stack_top - vm->stack) >= vm->stack_capacity) {
        ERROR_LOG("Stack overflow");
        vm->last_result = INTERPRET_RUNTIME_ERROR;
        return;
    }
    *vm->stack_top = value;
    vm->stack_top++;
}

Value pop(VM* vm) {
    if (vm->stack_top == vm->stack) {
        ERROR_LOG("Stack underflow");
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
        ERROR_LOG("Memory allocation failed for string concatenation");
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

static ObjUpvalue* captureUpvalue(VM* vm, Value* local) {
    ObjUpvalue* prev = NULL;
    ObjUpvalue* curr = vm->open_upvalues;

    // Find the right place to insert the new upvalue (sorted by location)
    while (curr != NULL && curr->location > local) {
        prev = curr;
        curr = curr->next;
    }

    // Found an existing upvalue for the same local variable, reuse it
    if (curr != NULL && curr->location == local) {
        return curr;
    }

    // Not found, create a new upvalue and insert it into the list
    ObjUpvalue* new = newUpvalue(vm, local);
    new->next = curr;

    if (prev == NULL) {
        vm->open_upvalues = new;
    } else {
        prev->next = new;
    }

    return new;
}

static void closeUpvalue(VM* vm, Value* last) {
    while (vm->open_upvalues != NULL && vm->open_upvalues->location >= last) {
        ObjUpvalue* upvalue = vm->open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->open_upvalues = upvalue->next;
    }
}

static bool duplicateString(VM* vm, Value a, Value b) {
    if (!IS_STRING(a) || !IS_NUMBER(b)) {
        ERROR_LOG("Type error: Expected string and number for duplication");
        return false;
    }

    ObjString* str = AS_STRING(a);
    double count_double = AS_NUMBER(b);

    if (count_double < 0 || trunc(count_double) != count_double) {
        ERROR_LOG(
            "Value error: Duplication count must be a non-negative integer");
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
        ERROR_LOG("Memory allocation failed for string duplication");
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

void printStack(VM* vm) {
    DEBUG_LOG("VM stack:");
    int i = 0;
    for (Value* slot = vm->stack; slot < vm->stack_top; slot++) {
        char* str = sprintValue(*slot);
        DEBUG_LOG("  %04d: [ %s ]", i++, str);
        free(str);
    }
}

void printConsts(Chunk* chunk) {
    DEBUG_LOG("Constants:");
    for (int i = 0; i < chunk->constants.count; i++) {
        char* str = sprintValue(chunk->constants.values[i]);
        DEBUG_LOG("  %04d: %s", i, str);
        free(str);
    }
}

// --- VM Execution (Direct Threading) ---

static int loadThreadedCode(ObjFunction* function, void* dispatch_table[]) {
    int result = 0;
    if (function->loaded_code != NULL) {
        return 0;  // Already loaded
    }
    Chunk* chunk = &function->chunk;

    DEBUG_LOG(
        "Loading function '%s' with %d bytecode instructions and %d constants",
        function->name ? function->name->chars : "<unnamed>", chunk->count,
        chunk->constants.count);
    DEBUG_CHUNK("\n%s", chunk);

    // "Loader" stage: Convert byte-based chunk to pointer-based code.
    size_t loaded_code_size = sizeof(void*) * chunk->count;
    void** loaded_code = (void**)malloc(loaded_code_size);
    if (!loaded_code) {
        ERROR_LOG("Memory error loading threaded code");
        result = -1;
        goto LOADER_CLEANUP;
    }

    int* byte_to_slot_map = malloc(sizeof(int) * chunk->count);
    if (byte_to_slot_map == NULL) {
        ERROR_LOG("Memory error allocating byte-to-slot map");
        result = -1;
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

    DEBUG_LOG("Loader first pass: translating bytecode to threaded code");
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
                        ERROR_LOG("Memory error allocating jumps to patch");
                        result = -1;
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
            case OP_CALL: {
                uint8_t arg_count = *bytecode++;
                loaded_code[loaded_idx++] = (void*)(uintptr_t)arg_count;
                break;
            }
            case OP_GET_LOCAL:
            case OP_SET_LOCAL: {
                uint8_t local_slot = *bytecode++;
                loaded_code[loaded_idx++] = (void*)(uintptr_t)local_slot;
                break;
            }
            case OP_CLOSURE: {
                uint16_t const_index =
                    (uint16_t)(bytecode[0] << 8) | bytecode[1];
                bytecode += 2;
                loaded_code[loaded_idx++] =
                    (void*)&chunk->constants.values[const_index];

                ObjFunction* fn =
                    AS_FUNCTION(chunk->constants.values[const_index]);
                for (int i = 0; i < fn->upvalue_cnt; i++) {
                    uint8_t is_local = *bytecode++;
                    uint8_t index = *bytecode++;
                    loaded_code[loaded_idx++] =
                        (void*)(uintptr_t)(is_local ? 1 : 0);
                    loaded_code[loaded_idx++] = (void*)(uintptr_t)index;
                }
                break;
            }
            case OP_GET_UPVALUE:
            case OP_SET_UPVALUE: {
                uint8_t upvalue_slot = *bytecode++;
                loaded_code[loaded_idx++] = (void*)(uintptr_t)upvalue_slot;
                break;
            }
            case OP_TAIL_CALL: {
                uint8_t arg_cnt = *bytecode++;
                loaded_code[loaded_idx++] = (void*)(uintptr_t)arg_cnt;
                break;
            }
            default:
                break;  // No operands
        }
    }
    loaded_code = reallocate(loaded_code, sizeof(void*) * chunk->count,
                             sizeof(void*) * loaded_idx);
    if (loaded_code == NULL) {
        ERROR_LOG("Memory error resizing loaded code");
        result = -1;
        goto LOADER_CLEANUP;
    }

    DEBUG_LOG("Loader second pass: Patching jump offsets");
    for (int i = 0; i < jump_count; i++) {
        int operand_slot_ix = jumps_to_patch[i];
        int target_byte_addr = (int)(uintptr_t)loaded_code[operand_slot_ix];
        int target_slot_ix = byte_to_slot_map[target_byte_addr];
        if (target_slot_ix == -1) {
            ERROR_LOG("Invalid jump target during patching");
            result = -1;
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
    if (result != 0) {
        reallocate(loaded_code, sizeof(void*) * loaded_idx, 0);
        function->loaded_code = NULL;
    } else {
        function->loaded_code_size = loaded_idx;
        function->loaded_code = loaded_code;
    }
    return result;
}

static InterpretResult run(VM* vm) {
#define BINARY_OP(op)                                       \
    do {                                                    \
        Value b = pop(vm);                                  \
        Value a = pop(vm);                                  \
        push(vm, NUMBER_VAL(AS_NUMBER(a) op AS_NUMBER(b))); \
    } while (false)

#define COMPARISON_OP(op)                                 \
    do {                                                  \
        Value b = pop(vm);                                \
        Value a = pop(vm);                                \
        if (a.type != b.type) {                           \
            ERROR_LOG("Comparison type mismatch error");  \
            result = INTERPRET_RUNTIME_ERROR;             \
            goto RETURN;                                  \
        }                                                 \
        push(vm, BOOL_VAL(AS_NUMBER(a) op AS_NUMBER(b))); \
    } while (false)

#define READ_BYTE() (*ip++)
#define READ_SHORT() (uint16_t)((*ip++ << 8) | (*ip++))

    // The dispatch table: an array of opcode implementation addresses.
    static void* dispatch_table[] = {
        &&OP_RETURN_IMPL,      &&OP_CONSTANT_IMPL,      &&OP_POP_IMPL,
        &&OP_JUMP_IMPL,        &&OP_JUMP_IF_FALSE_IMPL, &&OP_ADD_IMPL,
        &&OP_SUBTRACT_IMPL,    &&OP_MULTIPLY_IMPL,      &&OP_DIVIDE_IMPL,
        &&OP_NEGATE_IMPL,      &&OP_TRUE_IMPL,          &&OP_FALSE_IMPL,
        &&OP_NULL_IMPL,        &&OP_NOT_IMPL,           &&OP_EQUAL_IMPL,
        &&OP_GREATER_IMPL,     &&OP_LESS_IMPL,          &&OP_SET_GLOBAL_IMPL,
        &&OP_GET_GLOBAL_IMPL,  &&OP_CALL_IMPL,          &&OP_GET_LOCAL_IMPL,
        &&OP_SET_LOCAL_IMPL,   &&OP_CLOSURE_IMPL,       &&OP_GET_UPVALUE_IMPL,
        &&OP_SET_UPVALUE_IMPL, &&OP_TAIL_CALL_IMPL,
    };

    InterpretResult result = INTERPRET_OK;
    CallFrame* frame = &vm->frames[vm->frame_count - 1];
    if (frame->closure->function->loaded_code == NULL) {
        if (loadThreadedCode(frame->closure->function, dispatch_table) != 0) {
            result = INTERPRET_RUNTIME_ERROR;
            goto RETURN;
        }
        frame->ip = frame->closure->function->loaded_code;
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
    DEBUG_LOG("Starting direct-threaded execution");
    DISPATCH();

#define READ_CONSTANT() ((Value*)*frame->ip++)
#define READ_ARG() ((uintptr_t)*frame->ip++)

    DISPATCH();

    // --- Opcode Implementations ---

OP_RETURN_IMPL: {
    Value res = pop(vm);
    DEBUG_LOG(
        "OP_RETURN: FrameCount=%d (before decr), ret_val_type=%d ret_val=",
        vm->frame_count, res.type);
    DEBUG_VALUE("%s", res);
    closeUpvalue(vm, frame->slots);
    vm->last_popped_value = res;
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
        ERROR_LOG(
            "Runtime error: operands must be two numbers or two strings "
            "for addition");
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
        ERROR_LOG(
            "Runtime error: operands must be two numbers or a string and "
            "a number for multiplication");
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
        ERROR_LOG("Runtime error: operand must be a number for negation");
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
    Value name = frame->closure->function->chunk.constants.values[const_ix];
    tableInsert(&vm->globals, name, peek(vm, 0));
    DISPATCH();
}

OP_GET_GLOBAL_IMPL: {
    uint16_t const_ix = (uint16_t)READ_ARG();
    Value name = frame->closure->function->chunk.constants.values[const_ix];
    Value* value = tableGet(&vm->globals, name);
    if (value == NULL) {
        ERROR_LOG("Undefined variable '%.*s'", AS_STRING(name)->length,
                  AS_STRING(name)->chars);
        result = INTERPRET_RUNTIME_ERROR;
        goto RETURN;
    }
    push(vm, *value);
    DISPATCH();
}

OP_CALL_IMPL: {
    int arg_count = (int)READ_ARG();
    Value callee = peek(vm, arg_count);
    DEBUG_LOG(
        "[DEBUG] OP_CALL: FrameCount=%d, arg_count=%d, callee_type=%d, "
        "callee_value=",
        vm->frame_count, arg_count, callee.type);
    DEBUG_VALUE("%s", callee);

    if (!IS_OBJ(callee) || OBJ_TYPE(callee) != OBJ_CLOSURE) {
        ERROR_LOG("Runtime error: can only call functions");
        printStack(vm);
        printConsts(&frame->closure->function->chunk);
        result = INTERPRET_RUNTIME_ERROR;
        goto RETURN;
    }

    ObjClosure* closure = AS_CLOSURE(callee);
    if (arg_count != closure->function->arity) {
        ERROR_LOG(
            "Function %s: runtime error: expected %d arguments but got %d",
            closure->function->name, closure->function->arity, arg_count);
        result = INTERPRET_RUNTIME_ERROR;
        goto RETURN;
    }

    if (vm->frame_count == FRAMES_MAX) {
        ERROR_LOG("Runtime error: stack overflow (too many call frames)");
        result = INTERPRET_RUNTIME_ERROR;
        goto RETURN;
    }

    if (closure->function->loaded_code == NULL) {
        if (loadThreadedCode(closure->function, dispatch_table) != 0) {
            result = INTERPRET_RUNTIME_ERROR;
            goto RETURN;
        }
    }
    frame = &vm->frames[vm->frame_count++];
    frame->closure = closure;
    frame->slots = vm->stack_top - arg_count - 1;
    frame->ip = closure->function->loaded_code;

    // NOTE: LLDB frame code inspect: memory read -f dec -t uint8_t -c 9
    // frame->function->chunk.code

    DISPATCH();
}

OP_GET_LOCAL_IMPL: {
    uint8_t slot = (uint8_t)READ_ARG();
    push(vm, frame->slots[slot]);
    DISPATCH();
}

OP_SET_LOCAL_IMPL: {
    uint8_t slot = (uint8_t)READ_ARG();
    frame->slots[slot] = peek(vm, 0);
    DISPATCH();
}

OP_CLOSURE_IMPL: {
    Value* fn_val = READ_CONSTANT();
    ObjFunction* fn = AS_FUNCTION(*fn_val);
    ObjClosure* closure = newClosure(vm, fn);
    push(vm, OBJ_VAL(closure));
    for (int i = 0; i < closure->upvalue_cnt; i++) {
        uint8_t is_local = (uint8_t)READ_ARG();
        uint8_t index = (uint8_t)READ_ARG();
        if (is_local) {
            closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
        } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
        }
    }
    DISPATCH();
}

OP_GET_UPVALUE_IMPL: {
    uint8_t slot = (uint8_t)READ_ARG();
    ObjUpvalue* upvalue = frame->closure->upvalues[slot];
    push(vm, *upvalue->location);
    DISPATCH();
}

OP_SET_UPVALUE_IMPL: {
    uint8_t slot = (uint8_t)READ_ARG();
    ObjUpvalue* upvalue = frame->closure->upvalues[slot];
    *upvalue->location = peek(vm, 0);
    DISPATCH();
}

OP_TAIL_CALL_IMPL: {
    int arg_cnt = (int)READ_ARG();
    Value callee = peek(vm, arg_cnt);

    if (!IS_OBJ(callee) || OBJ_TYPE(callee) != OBJ_CLOSURE) {
        ERROR_LOG("Runtime error: can only call functions");
        result = INTERPRET_RUNTIME_ERROR;
        goto RETURN;
    }

    ObjClosure* closure = AS_CLOSURE(callee);
    if (arg_cnt != closure->function->arity) {
        ERROR_LOG(
            "Function %s: runtime error: expected %d arguments but got %d",
            closure->function->name, closure->function->arity, arg_cnt);
        result = INTERPRET_RUNTIME_ERROR;
        goto RETURN;
    }

    closeUpvalue(vm, frame->slots);

    Value* src = vm->stack_top - arg_cnt - 1;
    Value* dest = frame->slots;

    memmove(dest, src, sizeof(Value) * (arg_cnt + 1));
    vm->stack_top = dest + arg_cnt + 1;

    frame->closure = closure;
    if (closure->function->loaded_code == NULL) {
        if (loadThreadedCode(closure->function, dispatch_table) != 0) {
            result = INTERPRET_RUNTIME_ERROR;
            goto RETURN;
        }
    }
    frame->ip = closure->function->loaded_code;
    DISPATCH();
}

RETURN:
    return result;

#else
// Fallback for compilers that don't support computed gotos (e.g., MSVC)
#warning "Computed gotos not supported, falling back to switch-based dispatch."
    // ... switch-based implementation would go here ...
    ERROR_LOG("Direct threading not supported by this compiler");
    return INTERPRET_RUNTIME_ERROR;
#endif
}

#undef DEBUG_VALUE
#undef DEBUG_CHUNK
