#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h" // For the VM->objects linked list
#include "gc.h"

// A helper for allocating any object type.
// It initializes the base Obj header and adds the new object to the VM's list.
static Obj* allocateObject(VM* vm, size_t size, ObjType type) {
    gc(vm); // Trigger GC before allocation
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    if (object == NULL) {
        // In a real VM, this should trigger a GC cycle before failing.
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }
    object->type = type;

    // Add to the VM's object list for GC tracking
    object->next = vm->objects;
    vm->objects = object;

    return object;
}

// --- Function ---

ObjFunction* newFunction(VM* vm) {
    ObjFunction* function = (ObjFunction*)allocateObject(vm, sizeof(ObjFunction), OBJ_FUNCTION);
    function->arity = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}


// --- String ---

static uint32_t hashString(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

// Helper for allocating the ObjString itself
static ObjString* allocateString(VM* vm, char* chars, int length, uint32_t hash) {
    ObjString* string = (ObjString*)allocateObject(vm, sizeof(ObjString), OBJ_STRING);
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

    char* heapChars = reallocate(NULL, 0, length + 1);
    if (heapChars == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocateString(vm, heapChars, length, hash);
}
