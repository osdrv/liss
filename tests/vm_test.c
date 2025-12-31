#include "vm.h"

#include "minunit.h"
#include "value.h"

typedef struct {
    const char* name;
    const char* src;
    InterpretResult expected_result;
    Value expected_value;
} VMTestCase;

static char* test_vm_stack(void) {
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

static char* test_vm_interpret(void) {
    static VMTestCase tests[] = {{.name = "literal number",
                                  .src = "123",
                                  .expected_result = INTERPRET_OK,
                                  .expected_value = NUMBER_VAL(123.0)},
                                 {.name = "simple addition",
                                  .src = "(+ 1 2)",
                                  .expected_result = INTERPRET_OK,
                                  .expected_value = NUMBER_VAL(3.0)},
                                 {
                                     .name = "nested expression",
                                     .src = "(- (+ 10 5) 3)",
                                     .expected_result = INTERPRET_OK,
                                     .expected_value = NUMBER_VAL(12.0),
                                 }};

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        VMTestCase* test = &tests[i];
        VM* vm = newVM(256);
        mu_assert("Failed to create VM", vm != NULL);

        InterpretResult result = interpret(vm, test->src);
        mu_assert("Interpretation result mismatch",
                  result == test->expected_result);

        if (result == INTERPRET_OK) {
            Value v = vm->last_value;
            mu_assert("Result type mismatch",
                      v.type == test->expected_value.type);
            if (IS_NUMBER(v)) {
                mu_assert("Result value mismatch",
                          AS_NUMBER(v) == AS_NUMBER(test->expected_value));
            } else {
                mu_assert("Unhandled result type in test", false);
            }
            // Add more type checks as needed
        }

        destroyVM(vm);
    }

    return NULL;
}

// The suite function, called by the main test runner.
void vm_suite(void) {
    printf("--- VM Suite ---\n");
    mu_run_test(test_vm_stack);
    mu_run_test(test_vm_interpret);
}
