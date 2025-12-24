/*
 * A minimal unit testing framework for C.
 *
 * Usage:
 * 1. Include this header in your test file.
 * 2. Write test functions with the signature `char* test_foo()`.
 * 3. Inside your test, use `mu_assert(message, test)` to check conditions.
 *    - If `test` is false, the assertion fails, and the `message` is returned.
 *    - If `test` is true, the test continues.
 * 4. A test function returns `NULL` on success or an error message string on failure.
 * 5. Use the `mu_run_test(test)` macro to run a test function. It prints a dot `.`
 *    on success or an error message on failure and keeps track of tests run.
 * 6. The `main` function in your test runner should look like this:
 *
 * int main(int argc, char **argv) {
 *     mu_run_test(test_foo);
 *     mu_run_test(test_bar);
 *     // ... more tests
 *
 *     if (tests_run > 0) {
 *         printf("ALL TESTS PASSED\n");
 *     }
 *     printf("Tests run: %d\n", tests_run);
 *
 *     return result != 0;
 * }
 */

#ifndef minunit_h
#define minunit_h

#include <stdio.h>

#define mu_assert(message, test) do { if (!(test)) return message; } while (0)
#define mu_run_test(test) do { \
    char *message = test(); \
    tests_run++; \
    if (message) { \
        printf("\x1b[31mFAIL\x1b[0m: %s in %s\n", message, #test); \
        result = 1; \
    } else { \
        printf("\x1b[32m.\x1b[0m"); \
    } \
} while (0)

extern int tests_run;
extern int result;

#endif
