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
        case EXPECT_BOOL:
            assert_msg =
                assert_bool(val, strcmp(tests[i].expected_str, "true") == 0);
            break;
        case EXPECT_NIL:
            assert_msg = assert_nil(val);
            break;
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

static char *test_re_match_quest(void) {
    TestCase tests[] = {
        {.name = "match? a+ matches aaa",
         .src = "(import re [\"re\" \"match?\"]) (match? (re \"a+\") \"aaa\")",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
        {.name = "match? a+ does not match bbb",
         .src = "(import re [\"re\" \"match?\"]) (match? (re \"a+\") \"bbb\")",
         .expected_str = "false",
         .expected_type = EXPECT_BOOL},
        {.name = "match? a* matches empty string",
         .src = "(import re [\"re\" \"match?\"]) (match? (re \"a*\") \"\")",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
        {.name = "match? dot matches any char",
         .src = "(import re [\"re\" \"match?\"]) (match? (re \"a.b\") \"acb\")",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
        {.name = "match? dot requires exactly one char",
         .src = "(import re [\"re\" \"match?\"]) (match? (re \"a.b\") \"ab\")",
         .expected_str = "false",
         .expected_type = EXPECT_BOOL},
        {.name = "match? alternation matches first branch",
         .src = "(import re [\"re\" \"match?\"]) (match? (re \"a|b\") \"a\")",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
        {.name = "match? alternation matches second branch",
         .src = "(import re [\"re\" \"match?\"]) (match? (re \"a|b\") \"b\")",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
        {.name = "match? alternation does not match neither branch",
         .src = "(import re [\"re\" \"match?\"]) (match? (re \"a|b\") \"c\")",
         .expected_str = "false",
         .expected_type = EXPECT_BOOL},
        {.name = "match? group matches",
         .src = "(import re [\"re\" \"match?\"]) (match? (re \"(a)b\") \"ab\")",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_re_match(void) {
    TestCase tests[] = {
        {.name = "match returns whole match as group 0",
         .src = "(import re [\"re\" \"match\"]) (match (re \"a+\") \"aaa\")",
         .expected_str = "[\"aaa\"]",
         .expected_type = EXPECT_LIST},
        {.name = "match returns null when no match",
         .src = "(import re [\"re\" \"match\"]) (match (re \"a+\") \"bbb\")",
         .expected_str = "null",
         .expected_type = EXPECT_NIL},
        {.name = "match returns capture groups",
         .src = "(import re [\"re\" \"match\"]) (match (re \"(a)(b)\") \"ab\")",
         .expected_str = "[\"ab\" \"a\" \"b\"]",
         .expected_type = EXPECT_LIST},
        {.name = "match with group between literals",
         .src =
             "(import re [\"re\" \"match\"]) (match (re \"(a)b(c)\") \"abc\")",
         .expected_str = "[\"abc\" \"a\" \"c\"]",
         .expected_type = EXPECT_LIST},
        {.name = "unmatched open paren raises",
         .src = "(import re [\"re\"]) (try (re \"(abc\"))",
         .expected_str = "Invalid regex pattern",
         .expected_type = EXPECT_ERROR},
        {.name = "unmatched close paren raises",
         .src = "(import re [\"re\"]) (try (re \"abc)\"))",
         .expected_str = "Invalid regex pattern",
         .expected_type = EXPECT_ERROR},
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

void modules_re_suite(void) {
    printf("--- RE Module Suite ---\n");
    mu_run_test(test_re_match_quest);
    mu_run_test(test_re_match);
}
