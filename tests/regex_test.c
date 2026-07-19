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

typedef struct {
    const char* pattern;
    const char* text;
    bool expected;
} ClassMatchTest;

static char* test_char_classes() {
    ClassMatchTest tests[] = {
        // \d
        {.pattern = "\\d",     .text = "5",      .expected = true},
        {.pattern = "\\d",     .text = "a",      .expected = false},
        {.pattern = "\\d+",    .text = "123",    .expected = true},
        {.pattern = "\\d+",    .text = "abc",    .expected = false},
        {.pattern = "a\\db",   .text = "a7b",    .expected = true},
        {.pattern = "a\\db",   .text = "axb",    .expected = false},
        // \w
        {.pattern = "\\w",     .text = "a",      .expected = true},
        {.pattern = "\\w",     .text = "Z",      .expected = true},
        {.pattern = "\\w",     .text = "9",      .expected = true},
        {.pattern = "\\w",     .text = "_",      .expected = true},
        {.pattern = "\\w",     .text = " ",      .expected = false},
        {.pattern = "\\w",     .text = "-",      .expected = false},
        {.pattern = "\\w+",    .text = "hello",  .expected = true},
        {.pattern = "\\w+",    .text = "foo_1",  .expected = true},
        // \W
        {.pattern = "\\W",     .text = " ",      .expected = true},
        {.pattern = "\\W",     .text = "!",      .expected = true},
        {.pattern = "\\W",     .text = "a",      .expected = false},
        {.pattern = "\\W",     .text = "3",      .expected = false},
        // \s / \S
        {.pattern = "\\s",        .text = " ",        .expected = true},
        {.pattern = "\\s",        .text = "\t",       .expected = true},
        {.pattern = "\\s",        .text = "\n",       .expected = true},
        {.pattern = "\\s",        .text = "a",        .expected = false},
        {.pattern = "\\S",        .text = "a",        .expected = true},
        {.pattern = "\\S",        .text = " ",        .expected = false},
        {.pattern = "\\s+",       .text = "   ",      .expected = true},
        // \t / \n as literals
        {.pattern = "a\\tb",      .text = "a\tb",     .expected = true},
        {.pattern = "a\\tb",      .text = "a b",      .expected = false},
        {.pattern = "a\\nb",      .text = "a\nb",     .expected = true},
        {.pattern = "a\\nb",      .text = "ab",       .expected = false},
        // mixed
        {.pattern = "\\d+:\\w+",  .text = "42:foo",   .expected = true},
        {.pattern = "\\d+:\\w+",  .text = "42:",       .expected = false},
        {.pattern = "\\w+\\W\\d", .text = "abc.7",    .expected = true},
        {.pattern = "\\S+\\s\\S", .text = "foo bar",  .expected = true},
        // ^ and $ anchors
        {.pattern = "^foo",       .text = "foobar",   .expected = true},
        {.pattern = "^foo",       .text = "xfoo",     .expected = false},
        {.pattern = "foo$",       .text = "foo",      .expected = true},
        {.pattern = "foo$",       .text = "foobar",   .expected = false},
        {.pattern = "^foo$",      .text = "foo",      .expected = true},
        {.pattern = "^foo$",      .text = "foobar",   .expected = false},
        {.pattern = "^\\d+$",     .text = "123",      .expected = true},
        {.pattern = "^\\d+$",     .text = "12x",      .expected = false},
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        char* postfix = re2postfix(tests[i].pattern);
        mu_assert("re2postfix returned NULL", postfix != NULL);
        ReProgram* prog = compileRegex(postfix);
        free(postfix);

        bool got = match(prog, tests[i].text);
        if (got != tests[i].expected) {
            printf("FAIL: pattern='%s' text='%s' expected=%s got=%s\n",
                   tests[i].pattern, tests[i].text,
                   tests[i].expected ? "match" : "no-match",
                   got ? "match" : "no-match");
            mu_assert("char class match mismatch", false);
        }

        free(prog->instrs);
        free(prog);
    }
    return NULL;
}

static char* test_bracket_classes() {
    ClassMatchTest tests[] = {
        // literal set
        {.pattern = "[abc]",      .text = "a",    .expected = true},
        {.pattern = "[abc]",      .text = "b",    .expected = true},
        {.pattern = "[abc]",      .text = "d",    .expected = false},
        // range
        {.pattern = "[a-z]",      .text = "m",    .expected = true},
        {.pattern = "[a-z]",      .text = "M",    .expected = false},
        {.pattern = "[0-9]",      .text = "5",    .expected = true},
        {.pattern = "[0-9]",      .text = "x",    .expected = false},
        // negation
        {.pattern = "[^abc]",     .text = "d",    .expected = true},
        {.pattern = "[^abc]",     .text = "a",    .expected = false},
        {.pattern = "[^a-z]",     .text = "A",    .expected = true},
        {.pattern = "[^a-z]",     .text = "a",    .expected = false},
        // multi-range
        {.pattern = "[a-zA-Z]",   .text = "Z",    .expected = true},
        {.pattern = "[a-zA-Z]",   .text = "5",    .expected = false},
        {.pattern = "[a-zA-Z0-9]",.text = "9",    .expected = true},
        // with quantifiers
        {.pattern = "[a-z]+",     .text = "hello",.expected = true},
        {.pattern = "[a-z]+",     .text = "HELLO",.expected = false},
        {.pattern = "[0-9]+",     .text = "007",  .expected = true},
        // anchored
        {.pattern = "^[A-Z][a-z]+$", .text = "Hello",  .expected = true},
        {.pattern = "^[A-Z][a-z]+$", .text = "hello",  .expected = false},
        {.pattern = "^[A-Z][a-z]+$", .text = "HeLLo",  .expected = false},
        // combined with escape classes
        {.pattern = "[a-z]+\\d",  .text = "abc3", .expected = true},
        {.pattern = "[a-z]+\\d",  .text = "abc",  .expected = false},
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        ReProgram* prog = compilePattern(tests[i].pattern);
        mu_assert("compilePattern returned NULL", prog != NULL);

        bool got = match(prog, tests[i].text);
        if (got != tests[i].expected) {
            printf("FAIL: pattern='%s' text='%s' expected=%s got=%s\n",
                   tests[i].pattern, tests[i].text,
                   tests[i].expected ? "match" : "no-match",
                   got ? "match" : "no-match");
            mu_assert("bracket class match mismatch", false);
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
    mu_run_test(test_char_classes);
    mu_run_test(test_bracket_classes);
}
