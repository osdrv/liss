#ifndef liss_memory_h
#define liss_memory_h

#include "common.h"

typedef struct VM VM;  // Forward declaration of VM to avoid circular dependency

// Calculates a new capacity for a dynamic array based on its old capacity.
#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

// A generic macro to reallocate an array to a new size.
// It handles the casting and sizing automatically.
#define GROW_ARRAY(type, vm, pointer, old_count, new_count)    \
    (type*)reallocate(vm, pointer, sizeof(type) * (old_count), \
                      sizeof(type) * (new_count))

// A macro to free an array's memory. It just uses reallocate with a new size of
// 0.
#define FREE_ARRAY(type, vm, pointer, old_count) \
    reallocate(vm, pointer, sizeof(type) * (old_count), 0)

// The core memory management function. It's a wrapper around realloc.
// - old_size = 0, new_size > 0: Allocate new block.
// - old_size > 0, new_size = 0: Free allocation.
// - old_size > 0, new_size > old_size: Grow allocation.
// - old_size > 0, new_size < old_size: Shrink allocation.
void* reallocate(VM* vm, void* pointer, size_t old_size, size_t new_size);

#endif
