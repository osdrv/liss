#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "minunit.h"
#include "test_common.h"
#include "value.h"
#include "vm.h"

typedef struct {
    const char* name;
    const char* src;
} ModuleProto;

typedef struct {
    const char* name;
    const ModuleProto module;
    const char* src;
    InterpretResult expected_result;
    ExpectedValue expected_value;
    bool is_failing;
} ModuleTestCase;

// Writes a string to a file named [name].liss
static void write_test_module(const char* name, const char* source) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s.liss", name);
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        perror("Failed to create test module");
        exit(1);
    }
    fprintf(file, "%s", source);
    fclose(file);
}

// Deletes the file named [name].liss
static void clean_test_module(const char* name) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s.liss", name);
    unlink(filename);
}

static ModuleTestCase module_tests[] = {
    {
        .name = "reading a const from a module",
        .module =
            {
                .name = "test_module",
                .src = "(let const 42)",
            },
        .src = "(import test_module)\
                test_module:const",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 42},
    },
};

static char* test_modules(void) {
    for (size_t i = 0; i < sizeof(module_tests) / sizeof(module_tests[0]);
         i++) {
        ModuleTestCase* test = &module_tests[i];
        DEBUG_LOG("Running module test: %s", test->name);
        VMOptions options = {
            .stack_capacity = 64,
            .gc_threshold = 1024,
            .heap_growth_factor = 2,
            .stress_gc = true,
        };
        VM* vm = newVM(options);
        mu_assert("Failed to create VM", vm != NULL);

        if (test->is_failing) {
            DEBUG_LOG("Set a breakpoint for a failing test");
            __builtin_debugtrap();
        }

        write_test_module(test->module.name, test->module.src);

        InterpretResult result = interpret(vm, test->src, NULL);

        clean_test_module(test->module.name);

        mu_assert("Interpretation result mismatch",
                  result == test->expected_result);

        if (result == INTERPRET_OK) {
            Value actual = vm->last_popped_value;
            ExpectedValue expected = test->expected_value;

            char* assert_msg = NULL;
            switch (expected.type) {
                case EXPECT_BOOL:
                    assert_msg = assert_bool(actual, expected.as.boolean);
                    break;
                case EXPECT_NIL:
                    assert_msg = assert_nil(actual);
                    break;
                case EXPECT_INT:
                    assert_msg = assert_int(actual, expected.as.integer);
                    break;
                case EXPECT_REAL:
                    assert_msg = assert_real(actual, expected.as.real);
                    break;
                case EXPECT_STRING:
                    assert_msg = assert_string(actual, expected.as.string);
                    break;
                case EXPECT_LIST:
                    assert_msg = assert_list(actual, expected.as.string);
                    break;
                case EXPECT_ERROR:
                    assert_msg = assert_error(actual, expected.as.string);
                    break;
                default:
                    mu_assert("Unknown expected value type in test", false);
            }
            mu_assert("Value assertion failed", assert_msg == NULL);
        }

        destroyVM(vm);
        return NULL;
    }
}

// --- Suite ---

void module_suite() {
    printf("\n--- Module Suite ---\n");
    mu_run_test(test_modules);
}
