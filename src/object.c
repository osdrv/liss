#include "object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gc.h"
#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h"  // For the VM->objects linked list

// A helper for allocating any object type.
// It initializes the base Obj header and adds the new object to the VM's list.
static Obj* allocateObject(VM* vm, size_t size, ObjType type) {
    gc(vm);  // Trigger GC before allocation
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    if (object == NULL) {
        // In a real VM, this should trigger a GC cycle before failing.
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }
    object->type = type;
    object->isMarked = false;

    // Add to the VM's object list for GC tracking
    object->next = vm->objects;
    vm->objects = object;

    return object;
}

ObjError* newError(VM* vm, const char* message) {
    ObjString* msg_str = copyString(vm, message, (int)strlen(message));
    push(vm, OBJ_VAL(msg_str));  // Temporarily push to protect from GC
    ObjError* error =
        (ObjError*)allocateObject(vm, sizeof(ObjError), OBJ_ERROR);
    error->message = msg_str;
    pop(vm);  // Pop after allocation
    return error;
}

// --- Function ---

ObjFunction* newFunction(VM* vm) {
    ObjFunction* function =
        (ObjFunction*)allocateObject(vm, sizeof(ObjFunction), OBJ_FUNCTION);
    function->arity = 0;
    function->upvalue_cnt = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    function->loaded_code = NULL;
    function->loaded_code_size = 0;
    return function;
}

ObjClosure* newClosure(VM* vm, ObjFunction* function) {
    ObjUpvalue** upvalues =
        reallocate(NULL, 0, sizeof(ObjUpvalue*) * function->upvalue_cnt);
    for (int i = 0; i < function->upvalue_cnt; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure* closure =
        (ObjClosure*)allocateObject(vm, sizeof(ObjClosure), OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_cnt = function->upvalue_cnt;
    return closure;
}

ObjNative* newNative(VM* vm, const char* name, int arity, NativeFn function) {
    ObjNative* native =
        (ObjNative*)allocateObject(vm, sizeof(ObjNative), OBJ_NATIVE);
    native->arity = arity;
    native->function = function;
    native->name = copyString(vm, name, (int)strlen(name));
    return native;
}

ObjUpvalue* newUpvalue(VM* vm, Value* slot) {
    ObjUpvalue* upvalue =
        (ObjUpvalue*)allocateObject(vm, sizeof(ObjUpvalue), OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->closed = NIL_VAL;
    upvalue->next = NULL;
    return upvalue;
}

// --- String ---

uint32_t hashString(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

// Helper for allocating the ObjString itself
static ObjString* allocateString(VM* vm, char* chars, int length,
                                 uint32_t hash) {
    ObjString* string =
        (ObjString*)allocateObject(vm, sizeof(ObjString), OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    return string;
}

ObjString* takeString(VM* vm, char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    return allocateString(vm, chars, length, hash);
}

ObjString* copyString(VM* vm, const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
    if (interned != NULL) {
        return interned;
    }

    char* heap_chars = reallocate(NULL, 0, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';

    ObjString* string = allocateString(vm, heap_chars, length, hash);
    push(vm, OBJ_VAL(string));  // Temporarily push to protect from GC
    DEBUG_LOG("Interning string: %s", string->chars);
    tableInsert(&vm->strings, OBJ_VAL(string), OBJ_VAL(string));
    pop(vm);  // Pop after insertion

    return string;
}
