#include "common.h"
#include "minunit.h"
#include "test_common.h"
#include "value.h"
#include "vm.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *src;
    const char *expected_str;
    ExpectedValueType expected_type;
} TestCase;

static char *run_tests(TestCase *tests, size_t count) {
    for (size_t i = 0; i < count; i++) {
        VMOptions options = defaultVMOptions();
        options.stress_gc = true;
        VM *vm = newVM(options);

        InterpretResult result = interpret(vm, tests[i].src, NULL);
        if (result != INTERPRET_OK) {
            printf("Failed test: %s (InterpretResult: %d)\n", tests[i].name,
                   result);
            mu_assert("Interpretation failed", false);
        }

        Value val = vm->last_popped_value;
        char *assert_msg = NULL;

        switch (tests[i].expected_type) {
        case EXPECT_INT:
            assert_msg = assert_int(val, atoll(tests[i].expected_str));
            break;
        case EXPECT_REAL:
            assert_msg = assert_real(val, atof(tests[i].expected_str));
            break;
        case EXPECT_BOOL:
            assert_msg =
                assert_bool(val, strcmp(tests[i].expected_str, "true") == 0);
            break;
        case EXPECT_NIL:
            assert_msg = assert_nil(val);
            break;
        case EXPECT_ERROR:
            assert_msg = assert_error(val, tests[i].expected_str);
            break;
        default:
            break;
        }

        if (assert_msg != NULL) {
            printf("Failed test: %s\n", tests[i].name);
            mu_assert(assert_msg, false);
        }
        destroyVM(vm);
    }
    return NULL;
}

static char *test_math_floor_ceil_round(void) {
    TestCase tests[] = {
        {.name = "floor positive real",
         .src = "(import math [\"floor\"]) (floor 2.7)",
         .expected_str = "2",
         .expected_type = EXPECT_REAL},
        {.name = "floor negative real",
         .src = "(import math [\"floor\"]) (floor -2.3)",
         .expected_str = "-3",
         .expected_type = EXPECT_REAL},
        {.name = "floor integer input",
         .src = "(import math [\"floor\"]) (floor 4)",
         .expected_str = "4",
         .expected_type = EXPECT_REAL},
        {.name = "ceil positive real",
         .src = "(import math [\"ceil\"]) (ceil 2.3)",
         .expected_str = "3",
         .expected_type = EXPECT_REAL},
        {.name = "ceil negative real",
         .src = "(import math [\"ceil\"]) (ceil -2.7)",
         .expected_str = "-2",
         .expected_type = EXPECT_REAL},
        {.name = "round down",
         .src = "(import math [\"round\"]) (round 2.4)",
         .expected_str = "2",
         .expected_type = EXPECT_REAL},
        {.name = "round up",
         .src = "(import math [\"round\"]) (round 2.6)",
         .expected_str = "3",
         .expected_type = EXPECT_REAL},
        {.name = "round half away from zero",
         .src = "(import math [\"round\"]) (round -2.5)",
         .expected_str = "-3",
         .expected_type = EXPECT_REAL},
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_math_abs_sqrt(void) {
    TestCase tests[] = {
        {.name = "abs negative real",
         .src = "(import math [\"abs\"]) (abs -3.5)",
         .expected_str = "3.5",
         .expected_type = EXPECT_REAL},
        {.name = "abs positive real",
         .src = "(import math [\"abs\"]) (abs 3.5)",
         .expected_str = "3.5",
         .expected_type = EXPECT_REAL},
        {.name = "abs zero",
         .src = "(import math [\"abs\"]) (abs 0)",
         .expected_str = "0",
         .expected_type = EXPECT_REAL},
        {.name = "abs negative int",
         .src = "(import math [\"abs\"]) (abs -5)",
         .expected_str = "5",
         .expected_type = EXPECT_REAL},
        {.name = "sqrt of perfect square 4",
         .src = "(import math [\"sqrt\"]) (sqrt 4)",
         .expected_str = "2",
         .expected_type = EXPECT_REAL},
        {.name = "sqrt of perfect square 9",
         .src = "(import math [\"sqrt\"]) (sqrt 9)",
         .expected_str = "3",
         .expected_type = EXPECT_REAL},
        {.name = "sqrt of zero",
         .src = "(import math [\"sqrt\"]) (sqrt 0)",
         .expected_str = "0",
         .expected_type = EXPECT_REAL},
        {.name = "sqrt of negative raises",
         .src = "(import math [\"sqrt\"]) (try (sqrt -1))",
         .expected_str = "sqrt of negative number is not defined",
         .expected_type = EXPECT_ERROR},
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_math_pow_fmod(void) {
    TestCase tests[] = {
        {.name = "pow 2^3",
         .src = "(import math [\"pow\"]) (pow 2 3)",
         .expected_str = "8",
         .expected_type = EXPECT_REAL},
        {.name = "pow 3^2",
         .src = "(import math [\"pow\"]) (pow 3 2)",
         .expected_str = "9",
         .expected_type = EXPECT_REAL},
        {.name = "pow n^0 is 1",
         .src = "(import math [\"pow\"]) (pow 5 0)",
         .expected_str = "1",
         .expected_type = EXPECT_REAL},
        {.name = "pow 2^-1 is 0.5",
         .src = "(import math [\"pow\"]) (pow 2 -1)",
         .expected_str = "0.5",
         .expected_type = EXPECT_REAL},
        {.name = "fmod 7 mod 3",
         .src = "(import math [\"fmod\"]) (fmod 7 3)",
         .expected_str = "1",
         .expected_type = EXPECT_REAL},
        {.name = "fmod exact division",
         .src = "(import math [\"fmod\"]) (fmod 6 3)",
         .expected_str = "0",
         .expected_type = EXPECT_REAL},
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_math_log(void) {
    TestCase tests[] = {
        {.name = "log of 1 is 0",
         .src = "(import math [\"log\"]) (log 1)",
         .expected_str = "0",
         .expected_type = EXPECT_REAL},
        {.name = "log2 of 1 is 0",
         .src = "(import math [\"log2\"]) (log2 1)",
         .expected_str = "0",
         .expected_type = EXPECT_REAL},
        {.name = "log2 of 4 is 2",
         .src = "(import math [\"log2\"]) (log2 4)",
         .expected_str = "2",
         .expected_type = EXPECT_REAL},
        {.name = "log2 of 8 is 3",
         .src = "(import math [\"log2\"]) (log2 8)",
         .expected_str = "3",
         .expected_type = EXPECT_REAL},
        {.name = "log of zero raises",
         .src = "(import math [\"log\"]) (try (log 0))",
         .expected_str = "log argument must be greater than zero",
         .expected_type = EXPECT_ERROR},
        {.name = "log2 of negative raises",
         .src = "(import math [\"log2\"]) (try (log2 -1))",
         .expected_str = "log2 argument must be greater than zero",
         .expected_type = EXPECT_ERROR},
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_math_trig(void) {
    TestCase tests[] = {
        {.name = "exp of 0 is 1",
         .src = "(import math [\"exp\"]) (exp 0)",
         .expected_str = "1",
         .expected_type = EXPECT_REAL},
        {.name = "sin of 0 is 0",
         .src = "(import math [\"sin\"]) (sin 0)",
         .expected_str = "0",
         .expected_type = EXPECT_REAL},
        {.name = "cos of 0 is 1",
         .src = "(import math [\"cos\"]) (cos 0)",
         .expected_str = "1",
         .expected_type = EXPECT_REAL},
        {.name = "tan of 0 is 0",
         .src = "(import math [\"tan\"]) (tan 0)",
         .expected_str = "0",
         .expected_type = EXPECT_REAL},
        {.name = "atan2 of (0, 1) is 0",
         .src = "(import math [\"atan2\"]) (atan2 0 1)",
         .expected_str = "0",
         .expected_type = EXPECT_REAL},
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_math_constants(void) {
    TestCase tests[] = {
        {.name = "floor(PI) is 3",
         .src = "(import math [\"PI\" \"floor\"]) (floor PI)",
         .expected_str = "3",
         .expected_type = EXPECT_REAL},
        {.name = "floor(E) is 2",
         .src = "(import math [\"E\" \"floor\"]) (floor E)",
         .expected_str = "2",
         .expected_type = EXPECT_REAL},
        {.name = "floor(TAU) is 6",
         .src = "(import math [\"TAU\" \"floor\"]) (floor TAU)",
         .expected_str = "6",
         .expected_type = EXPECT_REAL},
        {.name = "floor(SQRT2) is 1",
         .src = "(import math [\"SQRT2\" \"floor\"]) (floor SQRT2)",
         .expected_str = "1",
         .expected_type = EXPECT_REAL},
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

void modules_math_suite(void) {
    printf("--- Math Module Suite ---\n");
    mu_run_test(test_math_floor_ceil_round);
    mu_run_test(test_math_abs_sqrt);
    mu_run_test(test_math_pow_fmod);
    mu_run_test(test_math_log);
    mu_run_test(test_math_trig);
    mu_run_test(test_math_constants);
}
