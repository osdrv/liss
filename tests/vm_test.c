#include "vm.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "minunit.h"
#include "value.h"

typedef enum {
    EXPECT_BOOL,
    EXPECT_NIL,
    EXPECT_NUMBER,
    EXPECT_STRING,
} ExpectedValueType;

typedef struct {
    ExpectedValueType type;
    union {
        bool boolean;
        double number;
        const char* string;
    } as;
} ExpectedValue;

typedef struct {
    const char* name;
    const char* src;
    InterpretResult expected_result;
    ExpectedValue expected_value;
    bool is_failing;
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

static char* assert_number(Value value, double expected) {
    mu_assert("Value is not a number.", IS_NUMBER(value));
    mu_assert("Number value does not match expected.",
              AS_NUMBER(value) == expected);
    return NULL;
}

static char* assert_string(Value value, const char* expected) {
    mu_assert("Value is not an object.", IS_OBJ(value));
    mu_assert("Object is not a string.", OBJ_TYPE(value) == OBJ_STRING);
    ObjString* str = AS_STRING(value);
    mu_assert("String length does not match expected.",
              str->length == (int)strlen(expected));
    mu_assert("String contents do not match expected.",
              memcmp(str->chars, expected, str->length) == 0);
    return NULL;
}

static char* assert_bool(Value value, bool expected) {
    mu_assert("Value is not a boolean.", IS_BOOL(value));
    mu_assert("Boolean value does not match expected.",
              AS_BOOL(value) == expected);
    return NULL;
}

static char* assert_nil(Value value) {
    mu_assert("Value is not null.", IS_NIL(value));
    return NULL;
}

static VMTestCase interpret_tests[] = {
    {
        .name = "literal number",
        .src = "123",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NUMBER, .as.number = 123.0},
    },
    {
        .name = "simple addition",
        .src = "(+ 1 2)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NUMBER, .as.number = 3.0},
    },
    {
        .name = "nested expression",
        .src = "(- (+ 10 5) 3)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NUMBER, .as.number = 12.0},
    },
    {
        .name = "not true",
        .src = "(! true)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = false},
    },
    {
        .name = "not false",
        .src = "(! false)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "not null",
        .src = "(! null)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "not literal",
        .src = "(! 123)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = false},
    },
    {
        .name = "AND expression with 2 arguments",
        .src = "(and true false)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = false},
    },
    {
        .name = "AND expression with all true",
        .src = "(and true true true)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "OR expression with all false",
        .src = "(or false false false)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = false},
    },
    {
        .name = "OR expression with mixed operands",
        .src = "(or false false true false)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "cond expression with else branch",
        .src = "(cond false 123 456)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NUMBER, .as.number = 456.0},
    },
    {
        .name = "cond expression with then branch taken",
        .src = "(cond true 123 456)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NUMBER, .as.number = 123.0},
    },
    {
        .name = "cond expression with no else branch resolving to else",
        .src = "(cond false 123)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NIL},
    },
    {
        .name = "cond expression with no else branch resolving to then",
        .src = "(cond true 123)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NUMBER, .as.number = 123.0},
    },
    {
        .name = "equality true",
        .src = "(= 5 5)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "equality false",
        .src = "(= 5 10)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = false},
    },
    {
        .name = "inequality with identical types",
        .src = "(!= 5 10)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "inequality with identical values",
        .src = "(!= 5 5)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = false},
    },
    {
        .name = "inequality with different types",
        .src = "(!= 5 true)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "greater than true",
        .src = "(> 10 5)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "greater than false",
        .src = "(> 5 10)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = false},
    },
    {
        .name = "type mismatch in comparison",
        .src = "(> 5 null)",
        .expected_result = INTERPRET_RUNTIME_ERROR,
    },
    {
        .name = "greater or equal true",
        .src = "(>= 10 5)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "greater or equal equal",
        .src = "(>= 5 5)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "greater or equal false",
        .src = "(>= 5 10)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = false},
    },
    {
        .name = "type mismatch in comparison",
        .src = "(>= true 5)",
        .expected_result = INTERPRET_RUNTIME_ERROR,
    },
    {
        .name = "less than true",
        .src = "(< 5 10)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "less than false",
        .src = "(< 10 5)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = false},
    },
    {
        .name = "less or equal true",
        .src = "(<= 5 10)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "less or equal equal",
        .src = "(<= 5 5)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "less or equal false",
        .src = "(<= 10 5)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = false},
    },
    {
        .name = "type mismatch in comparison",
        .src = "(<= null 5)",
        .expected_result = INTERPRET_RUNTIME_ERROR,
    },
    {
        .name = "basic let expression",
        .src = "(let x 10)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NUMBER, .as.number = 10.0},
    },
    {
        .name = "let expression with arithmetic",
        .src = "(let y (+ 5 5))",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NUMBER, .as.number = 10.0},
    },
    {
        .name = "let expression with boolean",
        .src = "(let flag true)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "let expression with get global",
        .src = "(let a 42) (let b (+ a 1))",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NUMBER, .as.number = 43.0},
    },
    {
        .name = "string concatenation",
        .src = "(+ \"Hello, \" \"world!\")",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_STRING, .as.string = "Hello, world!"},
    },
    {
        .name = "string concatenation with 3+ operands",
        .src = "(+ \"The \" \"quick \" \"brown \" \"fox.\")",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_STRING, .as.string = "The quick brown fox."},
    },
    {
        .name = "string and number concatenation error",
        .src = "(+ \"Value: \" 42)",
        .expected_result = INTERPRET_RUNTIME_ERROR,
    },
    {
        .name = "string duplication",
        .src = "(* \"ha\" 3)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_STRING, .as.string = "hahaha"},
    },
    {
        .name = "string duplication with zero",
        .src = "(* \"test\" 0)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_STRING, .as.string = ""},
    },
    {
        .name = "string duplication with negative number error",
        .src = "(* \"oops\" -2)",
        .expected_result = INTERPRET_RUNTIME_ERROR,
    },
    {
        .name = "string duplication with non-number error",
        .src = "(* \"nope\" true)",
        .expected_result = INTERPRET_RUNTIME_ERROR,
    },
    {
        .name = "empty function call should return NULL",
        .src = "(fn foo []) (foo)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NIL},
    },
    {
        .name = "function call returning a local variable",
        .src = "(fn foo []"
               "  (let x 123)"
               "  x"
               ")"
               "(foo)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NUMBER, .as.number = 123.0},
    },
    {
        .name = "function call with parameters",
        .src = "(fn add [a b] (+ a b)) (add 10 20)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NUMBER, .as.number = 30.0},
    },
    {
        .name = "nested empty function calls",
        .src = "(fn outer []"
               "  (fn inner [])"
               "  (inner)"
               ")"
               "(outer)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NIL},
    },
    {
        .name = "nested function call",
        .src = "(fn one [a b]"
               "  (fn two [a b]"
               "    (+ a b)"
               "  )"
               "  (two a b)"
               ")"
               "(one 1 2)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NUMBER, .as.number = 3.0},
    },
    {
        .name = "basic closure",
        .src = "(fn outer [a b]"
               "  (fn inner []"
               "    (+ a b)"
               "  )"
               "  inner"
               ")"
               "(let closure (outer 10 20))"
               "(closure)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NUMBER, .as.number = 30.0},
    },
    {
        .name = "lambda closure",
        .src = "(fn mk_multiplier [x]"
               "         (fn [y] (* x y))"
               ")"
               "(let double (mk_multiplier 2))"
               "(double 5)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NUMBER, .as.number = 10.0},
    },
    {
        .name = "tail call optimization",
        .src = "(let count_down (fn [n]"
               "  (cond (= n 0) \"done\""
               "    (count_down (- n 1)))))"
               "(count_down 100000)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_STRING, .as.string = "done"},
    },
};

static char* test_vm_interpret(void) {
    for (size_t i = 0; i < sizeof(interpret_tests) / sizeof(interpret_tests[0]);
         i++) {
        VMTestCase* test = &interpret_tests[i];
        DEBUG_LOG("Running vm test: %s", test->name);
        VM* vm = newVM(256);
        mu_assert("Failed to create VM", vm != NULL);

        if (test->is_failing) {
            DEBUG_LOG("Set a breakpoint for a failing test");
            __builtin_debugtrap();
        }

        InterpretResult result = interpret(vm, test->src);
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
                case EXPECT_NUMBER:
                    assert_msg = assert_number(actual, expected.as.number);
                    break;
                case EXPECT_STRING:
                    assert_msg = assert_string(actual, expected.as.string);
                    break;
                default:
                    mu_assert("Unknown expected value type in test", false);
            }
            mu_assert("Value assertion failed", assert_msg == NULL);
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
