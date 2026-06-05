#include "regex.h"

#include "common.h"
#include "minunit.h"
#include "test_common.h"

typedef struct {
    const char* pattern;
    const char* expected;
} RegexPostfixTest;

static char* test_re2postfix() {
    RegexPostfixTest tests[] = {
        {"a", "a"},
        {"ab", "ab@"},
        {"a.b", "a.@b@"},
        {"abc", "ab@c@"},

        {"a*", "a*"},
        {"a+", "a+"},
        {"a?", "a?"},
        {"ab*", "ab*@"},
        {"(ab)*", "ab@*"},

        {"a|b", "ab|"},
        {"a|b|c", "ab|c|"},
        {"ab|cd", "ab@cd@|"},

        {"a|bc*", "abc*@|"},
        {"(a|b)c", "ab|c@"},
        {"(a|b)*c", "ab|*c@"},

        {"((a))", "a"},
        {"(a(b|c))", "abc|@"},
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        char* got = re2postfix(tests[i].pattern);

        if (strcmp(got, tests[i].expected) != 0) {
            DEBUG_LOG("regex bad transform: pattern '%s'", tests[i].pattern);
            DEBUG_LOG("  expected: %s", tests[i].expected);
            DEBUG_LOG("  got: %s", got);
            free(got);
            return "Postfix mismatch";
        }
        free(got);
    }
    return NULL;
}

void regex_suite() {
    printf("\n--- Regex Suite ---\n");
    mu_run_test(test_re2postfix);
}
