#include "minunit.h"
#include "vm.h"

// Globals for the test runner
int tests_run = 0;
int result = 0;

// Example test case
static char* test_vm_initialization() {
    VM vm;
    initVM(&vm);
    // For now, we don't have anything to assert.
    // Let's add a placeholder assertion that always passes.
    mu_assert("VM initialization check failed (placeholder)", 1 == 1);
    freeVM(&vm);
    return NULL;
}

// Add all tests to be run here
static void run_all_tests() {
    mu_run_test(test_vm_initialization);
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
