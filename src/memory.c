#include "memory.h"

#include <stdlib.h>

#include "gc.h"
#include "vm.h"

void* reallocate(VM* vm, void* pointer, size_t old_size, size_t new_size) {
    (void)old_size;  // We don't need the old size for realloc itself, but it's
                     // good for custom allocators or debugging.

    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    if (vm != NULL) {
        vm->bytes_allocated += new_size - old_size;
        if (new_size > old_size) {
            if (vm->options.stress_gc || vm->bytes_allocated > vm->next_gc) {
                gc(vm);
            }
        }
    }

    void* result = realloc(pointer, new_size);
    if (result == NULL) {
        // Not enough memory, allocation failed.
        exit(1);
    }

    return result;
}
