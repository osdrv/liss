#include <stdio.h>

#include "minunit.h"

// --- Global Test State ---
int tests_run = 0;
int result = 0;

// --- Test Suite Declarations ---
// The main test runner will call these functions to execute tests from each
// suite.
void vm_suite(void);
void scanner_suite(void);
void compiler_suite(void);

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("--- Running Test Suites ---\n");

    // Call all the test suites
    vm_suite();
    scanner_suite();
    compiler_suite();

    printf("\n---------------------------\n");
    if (result == 0) {
        printf("ALL TESTS PASSED\n");
    } else {
        printf("SOME TESTS FAILED\n");
    }
    printf("Tests run: %d\n", tests_run);

    return result;
}
