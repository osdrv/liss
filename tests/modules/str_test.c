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
} StrTestCase;

static char *run_str_tests(StrTestCase *tests, size_t count) {
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
        case EXPECT_STRING: {
            mu_assert("Value is not string", IS_STRING(val));
            char *s = sprintValue(val);
            mu_assert("String mismatch",
                      strcmp(s, tests[i].expected_str) == 0);
            free(s);
        } break;
        case EXPECT_LIST:
            assert_msg = assert_list(val, tests[i].expected_str);
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

static char *test_str_case(void) {
    StrTestCase tests[] = {
        {.name = "upper basic",
         .src = "(import str [\"upper\"]) (upper \"hello\")",
         .expected_str = "\"HELLO\"",
         .expected_type = EXPECT_STRING},
        {.name = "upper mixed case",
         .src = "(import str [\"upper\"]) (upper \"Hello World\")",
         .expected_str = "\"HELLO WORLD\"",
         .expected_type = EXPECT_STRING},
        {.name = "upper empty string",
         .src = "(import str [\"upper\"]) (upper \"\")",
         .expected_str = "\"\"",
         .expected_type = EXPECT_STRING},
        {.name = "upper already uppercase",
         .src = "(import str [\"upper\"]) (upper \"ABC\")",
         .expected_str = "\"ABC\"",
         .expected_type = EXPECT_STRING},
        {.name = "lower basic",
         .src = "(import str [\"lower\"]) (lower \"WORLD\")",
         .expected_str = "\"world\"",
         .expected_type = EXPECT_STRING},
        {.name = "lower mixed case",
         .src = "(import str [\"lower\"]) (lower \"Hello World\")",
         .expected_str = "\"hello world\"",
         .expected_type = EXPECT_STRING},
        {.name = "lower empty string",
         .src = "(import str [\"lower\"]) (lower \"\")",
         .expected_str = "\"\"",
         .expected_type = EXPECT_STRING},
    };
    return run_str_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_str_trim(void) {
    StrTestCase tests[] = {
        {.name = "trim leading spaces",
         .src = "(import str [\"trim\"]) (trim \"  hello\")",
         .expected_str = "\"hello\"",
         .expected_type = EXPECT_STRING},
        {.name = "trim trailing spaces",
         .src = "(import str [\"trim\"]) (trim \"hello  \")",
         .expected_str = "\"hello\"",
         .expected_type = EXPECT_STRING},
        {.name = "trim both sides",
         .src = "(import str [\"trim\"]) (trim \"  hello  \")",
         .expected_str = "\"hello\"",
         .expected_type = EXPECT_STRING},
        {.name = "trim no spaces",
         .src = "(import str [\"trim\"]) (trim \"hello\")",
         .expected_str = "\"hello\"",
         .expected_type = EXPECT_STRING},
        {.name = "trim all spaces",
         .src = "(import str [\"trim\"]) (trim \"   \")",
         .expected_str = "\"\"",
         .expected_type = EXPECT_STRING},
        {.name = "trim empty string",
         .src = "(import str [\"trim\"]) (trim \"\")",
         .expected_str = "\"\"",
         .expected_type = EXPECT_STRING},
    };
    return run_str_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_str_search(void) {
    StrTestCase tests[] = {
        {.name = "contains found",
         .src =
             "(import str [\"contains?\"]) (contains? \"hello world\" \"world\")",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
        {.name = "contains not found",
         .src = "(import str [\"contains?\"]) (contains? \"hello\" \"xyz\")",
         .expected_str = "false",
         .expected_type = EXPECT_BOOL},
        {.name = "contains empty needle",
         .src = "(import str [\"contains?\"]) (contains? \"hello\" \"\")",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
        {.name = "starts_with true",
         .src = "(import str [\"starts_with?\"]) (starts_with? \"hello\" \"hel\")",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
        {.name = "starts_with false",
         .src =
             "(import str [\"starts_with?\"]) (starts_with? \"hello\" \"world\")",
         .expected_str = "false",
         .expected_type = EXPECT_BOOL},
        {.name = "starts_with needle longer than haystack",
         .src =
             "(import str [\"starts_with?\"]) (starts_with? \"hi\" \"hello\")",
         .expected_str = "false",
         .expected_type = EXPECT_BOOL},
        {.name = "starts_with exact match",
         .src =
             "(import str [\"starts_with?\"]) (starts_with? \"hello\" \"hello\")",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
        {.name = "ends_with true",
         .src = "(import str [\"ends_with?\"]) (ends_with? \"hello\" \"llo\")",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
        {.name = "ends_with false",
         .src = "(import str [\"ends_with?\"]) (ends_with? \"hello\" \"hel\")",
         .expected_str = "false",
         .expected_type = EXPECT_BOOL},
        {.name = "ends_with needle longer than haystack",
         .src = "(import str [\"ends_with?\"]) (ends_with? \"hi\" \"hello\")",
         .expected_str = "false",
         .expected_type = EXPECT_BOOL},
        {.name = "index_of at start",
         .src = "(import str [\"index_of\"]) (index_of \"hello\" \"h\")",
         .expected_str = "0",
         .expected_type = EXPECT_INT},
        {.name = "index_of in middle",
         .src = "(import str [\"index_of\"]) (index_of \"hello\" \"ll\")",
         .expected_str = "2",
         .expected_type = EXPECT_INT},
        {.name = "index_of not found",
         .src = "(import str [\"index_of\"]) (index_of \"hello\" \"xyz\")",
         .expected_str = "-1",
         .expected_type = EXPECT_INT},
        {.name = "index_of at end",
         .src = "(import str [\"index_of\"]) (index_of \"hello\" \"lo\")",
         .expected_str = "3",
         .expected_type = EXPECT_INT},
    };
    return run_str_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_str_substr(void) {
    StrTestCase tests[] = {
        {.name = "substr basic",
         .src = "(import str [\"substr\"]) (substr \"hello\" 1 3)",
         .expected_str = "\"ell\"",
         .expected_type = EXPECT_STRING},
        {.name = "substr from start",
         .src = "(import str [\"substr\"]) (substr \"hello\" 0 2)",
         .expected_str = "\"he\"",
         .expected_type = EXPECT_STRING},
        {.name = "substr length clamped to end",
         .src = "(import str [\"substr\"]) (substr \"hello\" 3 100)",
         .expected_str = "\"lo\"",
         .expected_type = EXPECT_STRING},
        {.name = "substr zero length",
         .src = "(import str [\"substr\"]) (substr \"hello\" 2 0)",
         .expected_str = "\"\"",
         .expected_type = EXPECT_STRING},
        {.name = "substr start out of bounds",
         .src = "(import str [\"substr\"]) (substr \"hello\" 10 2)",
         .expected_str = "substr: start out of bounds",
         .expected_type = EXPECT_ERROR},
        {.name = "substr negative length",
         .src = "(import str [\"substr\"]) (substr \"hello\" 1 -1)",
         .expected_str = "substr: length must be non-negative",
         .expected_type = EXPECT_ERROR},
    };
    return run_str_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_str_replace(void) {
    StrTestCase tests[] = {
        {.name = "replace basic",
         .src =
             "(import str [\"replace\"]) (replace \"hello world\" \"world\" "
             "\"there\")",
         .expected_str = "\"hello there\"",
         .expected_type = EXPECT_STRING},
        {.name = "replace not found returns original",
         .src =
             "(import str [\"replace\"]) (replace \"hello\" \"xyz\" \"abc\")",
         .expected_str = "\"hello\"",
         .expected_type = EXPECT_STRING},
        {.name = "replace first occurrence only",
         .src = "(import str [\"replace\"]) (replace \"aaa\" \"a\" \"b\")",
         .expected_str = "\"baa\"",
         .expected_type = EXPECT_STRING},
        {.name = "replace with empty string",
         .src =
             "(import str [\"replace\"]) (replace \"hello world\" \"world\" "
             "\"\")",
         .expected_str = "\"hello \"",
         .expected_type = EXPECT_STRING},
        {.name = "replace_all all occurrences",
         .src =
             "(import str [\"replace_all\"]) (replace_all \"aaa\" \"a\" \"b\")",
         .expected_str = "\"bbb\"",
         .expected_type = EXPECT_STRING},
        {.name = "replace_all multiple in sentence",
         .src = "(import str [\"replace_all\"]) (replace_all \"hello\" \"l\" "
                "\"L\")",
         .expected_str = "\"heLLo\"",
         .expected_type = EXPECT_STRING},
        {.name = "replace_all no match returns original",
         .src = "(import str [\"replace_all\"]) (replace_all \"hello\" \"xyz\" "
                "\"abc\")",
         .expected_str = "\"hello\"",
         .expected_type = EXPECT_STRING},
        {.name = "replace_all expanding replacement",
         .src = "(import str [\"replace_all\"]) (replace_all \"aaa\" \"a\" "
                "\"bb\")",
         .expected_str = "\"bbbbbb\"",
         .expected_type = EXPECT_STRING},
        {.name = "replace_all empty from returns original",
         .src =
             "(import str [\"replace_all\"]) (replace_all \"hello\" \"\" \"x\")",
         .expected_str = "\"hello\"",
         .expected_type = EXPECT_STRING},
    };
    return run_str_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_str_split(void) {
    StrTestCase tests[] = {
        {.name = "split basic",
         .src = "(import str [\"split\"]) (split \"a,b,c\" \",\")",
         .expected_str = "[\"a\" \"b\" \"c\"]",
         .expected_type = EXPECT_LIST},
        {.name = "split single element",
         .src = "(import str [\"split\"]) (split \"hello\" \",\")",
         .expected_str = "[\"hello\"]",
         .expected_type = EXPECT_LIST},
        {.name = "split empty delimiter splits into chars",
         .src = "(import str [\"split\"]) (split \"abc\" \"\")",
         .expected_str = "[\"a\" \"b\" \"c\"]",
         .expected_type = EXPECT_LIST},
        {.name = "split consecutive delimiters produce empty segments",
         .src = "(import str [\"split\"]) (split \"a,,b\" \",\")",
         .expected_str = "[\"a\" \"\" \"b\"]",
         .expected_type = EXPECT_LIST},
        {.name = "split delimiter at start",
         .src = "(import str [\"split\"]) (split \",a\" \",\")",
         .expected_str = "[\"\" \"a\"]",
         .expected_type = EXPECT_LIST},
        {.name = "split delimiter at end",
         .src = "(import str [\"split\"]) (split \"a,\" \",\")",
         .expected_str = "[\"a\" \"\"]",
         .expected_type = EXPECT_LIST},
    };
    return run_str_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_str_join(void) {
    StrTestCase tests[] = {
        {.name = "join basic",
         .src = "(import str [\"join\"]) (join [\"a\" \"b\" \"c\"] \",\")",
         .expected_str = "\"a,b,c\"",
         .expected_type = EXPECT_STRING},
        {.name = "join single element",
         .src = "(import str [\"join\"]) (join [\"hello\"] \",\")",
         .expected_str = "\"hello\"",
         .expected_type = EXPECT_STRING},
        {.name = "join empty delimiter",
         .src = "(import str [\"join\"]) (join [\"a\" \"b\"] \"\")",
         .expected_str = "\"ab\"",
         .expected_type = EXPECT_STRING},
        {.name = "join multi-char delimiter",
         .src =
             "(import str [\"join\"]) (join [\"a\" \"b\" \"c\"] \" | \")",
         .expected_str = "\"a | b | c\"",
         .expected_type = EXPECT_STRING},
    };
    return run_str_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_str_convert(void) {
    StrTestCase tests[] = {
        {.name = "to_int valid positive",
         .src = "(import str [\"to_int\"]) (to_int \"42\")",
         .expected_str = "42",
         .expected_type = EXPECT_INT},
        {.name = "to_int valid negative",
         .src = "(import str [\"to_int\"]) (to_int \"-10\")",
         .expected_str = "-10",
         .expected_type = EXPECT_INT},
        {.name = "to_int zero",
         .src = "(import str [\"to_int\"]) (to_int \"0\")",
         .expected_str = "0",
         .expected_type = EXPECT_INT},
        {.name = "to_int invalid string",
         .src = "(import str [\"to_int\"]) (to_int \"abc\")",
         .expected_str = "to_int: invalid integer",
         .expected_type = EXPECT_ERROR},
        {.name = "to_int empty string",
         .src = "(import str [\"to_int\"]) (to_int \"\")",
         .expected_str = "to_int: empty string",
         .expected_type = EXPECT_ERROR},
        {.name = "to_int trailing garbage",
         .src = "(import str [\"to_int\"]) (to_int \"42abc\")",
         .expected_str = "to_int: invalid integer",
         .expected_type = EXPECT_ERROR},
        {.name = "to_real valid float",
         .src = "(import str [\"to_real\"]) (to_real \"1.5\")",
         .expected_str = "1.5",
         .expected_type = EXPECT_REAL},
        {.name = "to_real integer string",
         .src = "(import str [\"to_real\"]) (to_real \"42\")",
         .expected_str = "42",
         .expected_type = EXPECT_REAL},
        {.name = "to_real negative",
         .src = "(import str [\"to_real\"]) (to_real \"-0.5\")",
         .expected_str = "-0.5",
         .expected_type = EXPECT_REAL},
        {.name = "to_real invalid string",
         .src = "(import str [\"to_real\"]) (to_real \"abc\")",
         .expected_str = "to_real: invalid real",
         .expected_type = EXPECT_ERROR},
        {.name = "to_real empty string",
         .src = "(import str [\"to_real\"]) (to_real \"\")",
         .expected_str = "to_real: empty string",
         .expected_type = EXPECT_ERROR},
    };
    return run_str_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_core_str(void) {
    StrTestCase tests[] = {
        {.name = "str passthrough on string",
         .src = "(str \"hello\")",
         .expected_str = "\"hello\"",
         .expected_type = EXPECT_STRING},
        {.name = "str stringify integer",
         .src = "(str 42)",
         .expected_str = "\"42\"",
         .expected_type = EXPECT_STRING},
        {.name = "str stringify true",
         .src = "(str true)",
         .expected_str = "\"true\"",
         .expected_type = EXPECT_STRING},
        {.name = "str stringify false",
         .src = "(str false)",
         .expected_str = "\"false\"",
         .expected_type = EXPECT_STRING},
        {.name = "str stringify null",
         .src = "(str null)",
         .expected_str = "\"null\"",
         .expected_type = EXPECT_STRING},
    };
    return run_str_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

void str_suite(void) {
    printf("--- Str Module Suite ---\n");
    mu_run_test(test_str_case);
    mu_run_test(test_str_trim);
    mu_run_test(test_str_search);
    mu_run_test(test_str_substr);
    mu_run_test(test_str_replace);
    mu_run_test(test_str_split);
    mu_run_test(test_str_join);
    mu_run_test(test_str_convert);
    mu_run_test(test_core_str);
}
