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

static VMTestCase interpret_tests[] = {
    {.name = "literal number",
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
    },
    {
        .name = "not true",
        .src = "(! true)",
        .expected_result = INTERPRET_OK,
        .expected_value = BOOL_VAL(false),
    },
    {
        .name = "not false",
        .src = "(! false)",
        .expected_result = INTERPRET_OK,
        .expected_value = BOOL_VAL(true),
    },
    {
        .name = "not null",
        .src = "(! null)",
        .expected_result = INTERPRET_OK,
        .expected_value = BOOL_VAL(true),
    },
    {
        .name = "not literal",
        .src = "(! 123)",
        .expected_result = INTERPRET_OK,
        .expected_value = BOOL_VAL(false),
    },
    {
        .name = "AND expression with 2 arguments",
        .src = "(and true false)",
        .expected_result = INTERPRET_OK,
        .expected_value = BOOL_VAL(false),
    },
    {
        .name = "AND expression with all true",
        .src = "(and true true true)",
        .expected_result = INTERPRET_OK,
        .expected_value = BOOL_VAL(true),
    },
    {
        .name = "OR expression with all false",
        .src = "(or false false false)",
        .expected_result = INTERPRET_OK,
        .expected_value = BOOL_VAL(false),
    },
    {
        .name = "OR expression with mixed operands",
        .src = "(or false false true false)",
        .expected_result = INTERPRET_OK,
        .expected_value = BOOL_VAL(true),
    },
    {
        .name = "cond expression with else branch",
        .src = "(cond false 123 456)",
        .expected_result = INTERPRET_OK,
        .expected_value = NUMBER_VAL(456.0),
    },
    {
        .name = "cond expression with then branch taken",
        .src = "(cond true 123 456)",
        .expected_result = INTERPRET_OK,
        .expected_value = NUMBER_VAL(123.0),
    },
    {
        .name = "cond expression with no else branch resolving to else",
        .src = "(cond false 123)",
        .expected_result = INTERPRET_OK,
        .expected_value = NIL_VAL,
    },
    {
        .name = "cond expression with no else branch resolving to then",
        .src = "(cond true 123)",
        .expected_result = INTERPRET_OK,
        .expected_value = NUMBER_VAL(123.0),
    },
};

static char* test_vm_interpret(void) {
    for (size_t i = 0; i < sizeof(interpret_tests) / sizeof(interpret_tests[0]);
         i++) {
        VMTestCase* test = &interpret_tests[i];
        VM* vm = newVM(256);
        mu_assert("Failed to create VM", vm != NULL);

        InterpretResult result = interpret(vm, test->src);
        mu_assert("Interpretation result mismatch",
                  result == test->expected_result);

        if (result == INTERPRET_OK) {
            Value v = vm->last_popped_value;
            if (v.type != test->expected_value.type) {
                DEBUG_LOG("Expected type: %d, got type: %d",
                          test->expected_value.type, v.type);
            }
            mu_assert("Result type mismatch",
                      v.type == test->expected_value.type);
            if (IS_NUMBER(v)) {
                if (AS_NUMBER(v) != AS_NUMBER(test->expected_value)) {
                    DEBUG_LOG("here");
                }
                mu_assert("Numeric result value mismatch",
                          AS_NUMBER(v) == AS_NUMBER(test->expected_value));
            } else if (IS_BOOL(v)) {
                mu_assert("Boolean result value mismatch",
                          AS_BOOL(v) == AS_BOOL(test->expected_value));
            } else if (IS_NIL(v)) {
                mu_assert("Unexpected null result", test->expected_value.type == VAL_NIL);
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
