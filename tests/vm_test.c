#include "minunit.h"
#include "vm.h"
#include "value.h"

// Globals for the test runner
int tests_run = 0;
int result = 0;

// A more meaningful test for the new VM
static char* test_vm_stack() {
    VM* vm = newVM(16); // Create a VM with a small stack for testing
    mu_assert("Failed to create VM", vm != NULL);

    // Test push and pop
    push(vm, NUMBER_VAL(42.0));
    Value v = pop(vm);

    mu_assert("Popped value should be a number", IS_NUMBER(v));
    mu_assert("Popped value should be 42.0", AS_NUMBER(v) == 42.0);

    destroyVM(vm);
    return NULL;
}

// Add all tests to be run here
static void run_all_tests() {
    mu_run_test(test_vm_stack);
}

int main(int argc, char **argv) {
    (void)argc; // Suppress unused warning
    (void)argv; // Suppress unused warning

    run_all_tests();

    printf("\n");
    if (result == 0) {
        printf("ALL TESTS PASSED\n");
    }
    printf("Tests run: %d\n", tests_run);

    return result;
}
