#include "gc.h"

#include <stdio.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

void gc(VM* vm) {
    DEBUG_LOG("--- GC Begin ---");
    markRoots(vm);
    sweep(vm);
    size_t new_threshold = vm->bytes_allocated * vm->options.heap_growth_factor;
    vm->next_gc = (new_threshold < vm->options.gc_threshold)
                      ? vm->options.gc_threshold
                      : new_threshold;
    DEBUG_LOG("GC collected. New heap size: %zu bytes. Next GC at: %zu bytes.",
              vm->bytes_allocated, vm->next_gc);
    DEBUG_LOG("--- GC End ---");
}

void markRoots(VM* vm) {
    for (Value* value = vm->stack; value < vm->stack_top; value++) {
        markValue(vm, *value);
    }
    markTable(vm, &vm->globals);
    markTable(vm, &vm->strings);
    markValue(vm, vm->raise_value);
    markValue(vm, vm->empty_list);
    // mark upvalues
    for (ObjUpvalue* upvalue = vm->open_upvalues; upvalue != NULL;
         upvalue = upvalue->next) {
        markObject(vm, (Obj*)upvalue);
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
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            markObject(vm, (Obj*)closure->function);
            for (int i = 0; i < closure->upvalue_cnt; i++) {
                markObject(vm, (Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_UPVALUE: {
            ObjUpvalue* upvalue = (ObjUpvalue*)object;
            markValue(vm, upvalue->closed);
            break;
        }
        case OBJ_ERROR: {
            ObjError* error = (ObjError*)object;
            markObject(vm, (Obj*)error->message);
            break;
        }
        case OBJ_NATIVE:
            ObjNative* native = (ObjNative*)object;
            markObject(vm, (Obj*)native->name);
            break;
        case OBJ_PAIR: {
            ObjPair* pair = (ObjPair*)object;
            markValue(vm, pair->first);
            markValue(vm, pair->second);
            break;
        }
        case OBJ_LIST: {
            ObjList* list = (ObjList*)object;
            markValue(vm, list->head);
            break;
        }
        case OBJ_MODULE: {
            ObjModule* module = (ObjModule*)object;
            markObject(vm, (Obj*)module->name);
            markTable(vm, &module->imports);
            break;
        }
    }
}

void markTable(VM* vm, Table* table) {
    for (size_t i = 0; i < table->bucket_count; i++) {
        TableEntry* entry = table->buckets[i];
        while (entry != NULL) {
            markValue(vm, entry->key);
            markValue(vm, entry->value);
            entry = entry->next;
        }
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
            freeObject(vm, unreached);
        }
    }
}

void freeObject(VM* vm, Obj* object) {
    DEBUG_LOG("Freeing object %p type %d", (void*)object, object->type);
    switch (object->type) {
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            if (function->loaded_code != NULL) {
                FREE_ARRAY(void*, vm, function->loaded_code,
                           function->loaded_code_size);
            }
            freeChunk(vm, &function->chunk);
            reallocate(vm, function, sizeof(ObjFunction), 0);
            break;
        }
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            reallocate(vm, string->chars, string->length + 1, 0);
            reallocate(vm, string, sizeof(ObjString), 0);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            reallocate(vm, closure->upvalues,
                       sizeof(ObjUpvalue*) * closure->upvalue_cnt, 0);
            reallocate(vm, closure, sizeof(ObjClosure), 0);
            break;
        }
        case OBJ_UPVALUE: {
            ObjUpvalue* upvalue = (ObjUpvalue*)object;
            reallocate(vm, upvalue, sizeof(ObjUpvalue), 0);
            break;
        }
        case OBJ_ERROR: {
            ObjError* error = (ObjError*)object;
            reallocate(vm, error, sizeof(ObjError), 0);
            break;
        }
        case OBJ_NATIVE: {
            ObjNative* native = (ObjNative*)object;
            reallocate(vm, native, sizeof(ObjNative), 0);
            break;
        }
        case OBJ_PAIR: {
            ObjPair* pair = (ObjPair*)object;
            reallocate(vm, pair, sizeof(ObjPair), 0);
            break;
        }
        case OBJ_LIST: {
            ObjList* list = (ObjList*)object;
            reallocate(vm, list, sizeof(ObjList), 0);
            break;
        }
        case OBJ_MODULE: {
            ObjModule* module = (ObjModule*)object;
            reallocate(vm, module, sizeof(ObjModule), 0);
            break;
        }
    }
}
