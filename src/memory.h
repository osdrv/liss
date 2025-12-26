#ifndef liss_memory_h
#define liss_memory_h

#include "common.h"

// Calculates a new capacity for a dynamic array based on its old capacity.
#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

// A generic macro to reallocate an array to a new size.
// It handles the casting and sizing automatically.
#define GROW_ARRAY(type, pointer, oldCount, newCount)     \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
                      sizeof(type) * (newCount))

// A macro to free an array's memory. It just uses reallocate with a new size of
// 0.
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

// The core memory management function. It's a wrapper around realloc.
// - oldSize = 0, newSize > 0: Allocate new block.
// - oldSize > 0, newSize = 0: Free allocation.
// - oldSize > 0, newSize > oldSize: Grow allocation.
// - oldSize > 0, newSize < oldSize: Shrink allocation.
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

#endif
