#ifndef liss_gc_h
#define liss_gc_h

#include "object.h"
#include "value.h"
#include "vm.h"

void gc(VM* vm);
void markObject(VM* vm, Obj* object);
void markTable(VM* vm, Table* table);
void markRoots(VM* vm);
void markValue(VM* vm, Value value);
void sweep(VM* vm);
void freeObject(VM* vm, Obj* object);

#endif
