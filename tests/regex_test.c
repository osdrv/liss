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
        {"(ab)*", "ab@\x{01}*"},

        {"a|b", "ab|"},
        {"a|b|c", "ab|c|"},
        {"ab|cd", "ab@cd@|"},

        {"a|bc*", "abc*@|"},
        {"(a|b)c", "ab|\x{01}c@"},
        {"(a|b)*c", "ab|\x{01}*c@"},

        {"((a))", "a\x{02}\x{01}"},
        {"(a(b|c))", "abc|\x{02}@\x{01}"},
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

typedef struct {
    const char* pattern;
    const char* text;
    bool expected_match;
    int expected_groups;
    struct {
        int start;
        int end;
    } groups[MAX_GROUPS];
    bool debug;
} RegexGroupTest;

static char* test_match_groups() {
    RegexGroupTest tests[] = {{.pattern = "(a)b(c)",
                               .text = "abc",
                               .expected_match = true,
                               .expected_groups = 3,
                               .groups = {{0, 3},  // Group 0: "abc"
                                          {0, 1},  // Group 1: "a"
                                          {2, 3},  // Group 2: "c"
                                          {-1, -1}}},
                              {.pattern = "a(b*)c",
                               .text = "abbbc",
                               .expected_match = true,
                               .expected_groups = 2,
                               .groups = {{0, 5},  // Group 0: "abbbc"
                                          {1, 4},  // Group 1: "bbb"
                                          {-1, -1}}},
                              {.pattern = "a(b|c)?d",
                               .text = "ad",
                               .expected_match = true,
                               .expected_groups = 2,
                               .groups = {{0, 2},    // Group 0: "ad"
                                          {-1, -1},  // Group 1: did not match
                                          {-1, -1}}},
                              {.pattern = "(a(b))",
                               .text = "ab",
                               .expected_match = true,
                               .expected_groups = 3,
                               .groups = {{0, 2},  // Group 0: "ab"
                                          {0, 2},  // Group 1: "ab"
                                          {1, 2},  // Group 2: "b"
                                          {-1, -1}}}};

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        char* postfix = re2postfix(tests[i].pattern);
        ReProgram* prog = compileRegex(postfix);
        free(postfix);

        if (tests[i].debug) {
            DEBUG_LOG("Set a breakpoint for a failing test");
            __builtin_debugtrap();
        }

        const char* submatch[MAX_GROUPS * 2];
        bool got_match = matchGroups(prog, tests[i].text, submatch);

        DEBUG_LOG("testing pattern: %s", tests[i].pattern);

        mu_assert("match result mismatch",
                  got_match == tests[i].expected_match);

        if (got_match) {
            mu_assert("number of groups mismatch",
                      prog->num_grps == tests[i].expected_groups);

            for (int g = 0; g < tests[i].expected_groups; g++) {
                int exp_start = tests[i].groups[g].start;
                int exp_end = tests[i].groups[g].end;

                if (exp_start == -1) {
                    mu_assert("expected group not to match",
                              submatch[2 * g] == NULL);
                } else {
                    int got_start = submatch[2 * g] - tests[i].text;
                    int got_end = submatch[2 * g + 1] - tests[i].text;
                    mu_assert("group start offset mismatch",
                              got_start == exp_start);
                    mu_assert("group end offset mismatch", got_end == exp_end);
                }
            }
        }

        free(prog->instrs);
        free(prog);
    }
    return NULL;
}

void regex_suite() {
    printf("\n--- Regex Suite ---\n");
    mu_run_test(test_re2postfix);
    mu_run_test(test_match_groups);
}
