#include <stdio.h>
#include "gc.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

void gc(VM* vm) {
    DEBUG_LOG("--- GC Begin ---");
    markRoots(vm);
    sweep(vm);
    DEBUG_LOG("--- GC End ---");
}

void markRoots(VM* vm) {
    for (Value* value = vm->stack; value < vm->stack_top; value++) {
        markValue(vm, *value);
    }
}

void markValue(VM* vm, Value value) {
    if (IS_OBJ(value)) {
        markObject(vm, AS_OBJ(value));
    }
}

void markObject(VM* vm, Obj* object) {
    if (object == NULL || object->isMarked) return;

    object->isMarked = true;
    DEBUG_LOG("Marked object %p", (void*)object);

    switch (object->type) {
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            markObject(vm, (Obj*)function->name);
            for (int i = 0; i < function->chunk.constants.count; i++) {
                markValue(vm, function->chunk.constants.values[i]);
            }
            break;
        }
        case OBJ_STRING:
            // Strings have no references to other objects.
            break;
    }
}

void sweep(VM* vm) {
    Obj* previous = NULL;
    Obj* object = vm->objects;
    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm->objects = object;
            }
            freeObject(unreached);
        }
    }
}

void freeObject(Obj* object) {
    DEBUG_LOG("Freeing object %p type %d", (void*)object, object->type);
    switch (object->type) {
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            reallocate(function, sizeof(ObjFunction), 0);
            break;
        }
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            reallocate(string->chars, string->length + 1, 0);
            reallocate(string, sizeof(ObjString), 0);
            break;
        }
    }
}
