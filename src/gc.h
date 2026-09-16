#ifndef liss_gc_h
#define liss_gc_h

#include "object.h"
#include "value.h"
#include "vm.h"

#define WRITE_BARRIER(vm, owner, value)                 \
    do {                                                \
        if (IS_OBJ(value) && (owner)->gen == GEN_OLD && \
            AS_OBJ(value)->gen == GEN_NEW) {            \
            rememberObject(vm, owner);                  \
        }                                               \
    } while (0)

void minorGC(VM* vm);
void gc(VM* vm);
void markNewObject(VM* vm, Obj* object);
void markObject(VM* vm, Obj* object);
void markNewTable(VM* vm, Table* table);
void markTable(VM* vm, Table* table);
void markRoots(VM* vm);
void markNewValue(VM* vm, Value value);
void markValue(VM* vm, Value value);
void sweep(VM* vm);
void freeObject(VM* vm, Obj* object);
void rememberObject(VM* vm, Obj* object);

#endif
