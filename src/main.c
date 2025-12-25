#include <stdio.h>
#include "vm.h"
#include "value.h"
#include "common.h"

int main(int argc, const char* argv[]) {
    (void)argc; (void)argv; // Suppress unused warnings

    // The VM is now heap-allocated using our factory.
    VM* vm = newVM(STACK_MAX);
    if (vm == NULL) {
        fprintf(stderr, "Could not create VM.\n");
        return 1;
    }

    DEBUG_LOG("Liss VM - Running with new Value system.");

    // --- Demo of the new stack and value system ---
    DEBUG_LOG("Pushing a number onto the stack...");
    push(vm, NUMBER_VAL(1.23));
    
    Value val = pop(vm);
    DEBUG_LOG("Popping and printing... Value: %g", AS_NUMBER(val));
    // --- End Demo ---

    // The destroy function now handles all cleanup.
    destroyVM(vm);
    
    return 0;
}
