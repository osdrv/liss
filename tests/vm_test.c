#include "vm.h"

#include "minunit.h"
#include "value.h"

// A more meaningful test for the new VM
static char* test_vm_stack() {
    VM* vm = newVM(16);  // Create a VM with a small stack for testing
    mu_assert("Failed to create VM", vm != NULL);

    // Test push and pop
    push(vm, NUMBER_VAL(42.0));
    Value v = pop(vm);

    mu_assert("Popped value should be a number", IS_NUMBER(v));
    mu_assert("Popped value should be 42.0", AS_NUMBER(v) == 42.0);

    destroyVM(vm);
    return NULL;
}

// The suite function, called by the main test runner.
void vm_suite() {
    printf("--- VM Suite ---\n");
    mu_run_test(test_vm_stack);
}
