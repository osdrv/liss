#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "common.h"
#include "vm.h"
#include "value.h"
#include "object.h"

// --- VM Lifecycle ---

VM* newVM(size_t stack_capacity) {
    // Allocate space for the VM struct plus the flexible array member for the stack.
    VM* vm = (VM*)malloc(sizeof(VM) + sizeof(Value) * stack_capacity);
    if (vm == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }

    vm->stack_capacity = stack_capacity;
    vm->stack_top = vm->stack; // Stack starts empty, top points to the beginning.
    vm->objects = NULL;        // No objects allocated yet.
    
    DEBUG_LOG("Initialized new VM with stack capacity %zu.", stack_capacity);
    return vm;
}

void destroyVM(VM* vm) {
    DEBUG_LOG("Destroying VM.");
    
    // Free all heap-allocated objects.
    Obj* object = vm->objects;
    while (object != NULL) {
        Obj* next = object->next;
        // A real implementation would have a freeObject function
        // that switches on the object type to free internal data.
        // For now, we only have ObjString which might have copied chars.
        if (object->type == OBJ_STRING) {
            ObjString* string = (ObjString*)object;
            // This assumes the string was created with copyString, not takeString.
            // A more robust system is needed for a real GC.
            free(string->chars);
        }
        free(object);
        object = next;
    }

    // Free the VM struct itself.
    free(vm);
}


// --- Stack Operations ---

void push(VM* vm, Value value) {
    if ((size_t)(vm->stack_top - vm->stack) >= vm->stack_capacity) {
        fprintf(stderr, "Stack overflow.\n");
        // In a real VM, you might want to handle this more gracefully.
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
        case VAL_BOOL:   printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NIL:    printf("nil"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
        case VAL_OBJ:
            switch(OBJ_TYPE(value)) {
                case OBJ_STRING:
                    printf("\"%s\"", AS_CSTRING(value));
                    break;
                // Future object types would have cases here.
            }
            break;
    }
}
