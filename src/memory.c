#include <stdlib.h>

#include "memory.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    (void)oldSize; // We don't need the old size for realloc itself, but it's good for custom allocators or debugging.

    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) {
        // Not enough memory, allocation failed.
        exit(1);
    }

    return result;
}
