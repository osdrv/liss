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
} ListTestCase;

static char *run_list_tests(ListTestCase *tests, size_t count) {
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

static char *test_list_head_tail_last(void) {
    ListTestCase tests[] = {
        {.name = "head of singleton",
         .src = "(import list [head]) (head [42])",
         .expected_str = "42",
         .expected_type = EXPECT_INT},
        {.name = "head of multi-element list",
         .src = "(import list [head]) (head [1 2 3])",
         .expected_str = "1",
         .expected_type = EXPECT_INT},
        {.name = "tail returns rest as list",
         .src = "(import list [tail]) (tail [1 2 3])",
         .expected_str = "[2 3]",
         .expected_type = EXPECT_LIST},
        {.name = "tail of singleton is empty list",
         .src = "(import list [tail]) (is_empty? (tail [1]))",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
        {.name = "last of singleton",
         .src = "(import list [last]) (last [99])",
         .expected_str = "99",
         .expected_type = EXPECT_INT},
        {.name = "last of multi-element list",
         .src = "(import list [last]) (last [1 2 3 4 5])",
         .expected_str = "5",
         .expected_type = EXPECT_INT},
        {.name = "head of empty list raises",
         .src = "(import list [head]) (try (head []))",
         .expected_str = "list:head: empty list",
         .expected_type = EXPECT_ERROR},
        {.name = "tail of empty list raises",
         .src = "(import list [tail]) (try (tail []))",
         .expected_str = "list:tail: empty list",
         .expected_type = EXPECT_ERROR},
        {.name = "last of empty list raises",
         .src = "(import list [last]) (try (last []))",
         .expected_str = "list:last: empty list",
         .expected_type = EXPECT_ERROR},
    };
    return run_list_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_list_cons_push(void) {
    ListTestCase tests[] = {
        {.name = "cons prepends element",
         .src = "(import list [cons]) (cons [2 3] 1)",
         .expected_str = "[1 2 3]",
         .expected_type = EXPECT_LIST},
        {.name = "cons onto empty list",
         .src = "(import list [cons]) (cons [] 7)",
         .expected_str = "[7]",
         .expected_type = EXPECT_LIST},
        {.name = "cons is non-destructive",
         .src = "(import list [cons head]) (let l [2 3]) (cons l 1) (head l)",
         .expected_str = "2",
         .expected_type = EXPECT_INT},
        {.name = "push appends element",
         .src = "(import list [push]) (push [1 2] 3)",
         .expected_str = "[1 2 3]",
         .expected_type = EXPECT_LIST},
        {.name = "push onto empty list",
         .src = "(import list [push]) (push [] 5)",
         .expected_str = "[5]",
         .expected_type = EXPECT_LIST},
        {.name = "push is non-destructive",
         .src = "(import list [push last]) (let l [1 2]) (push l 3) (last l)",
         .expected_str = "2",
         .expected_type = EXPECT_INT},
    };
    return run_list_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_list_append(void) {
    ListTestCase tests[] = {
        {.name = "append two lists",
         .src = "(import list [append]) (append [1 2] [3 4])",
         .expected_str = "[1 2 3 4]",
         .expected_type = EXPECT_LIST},
        {.name = "append empty left",
         .src = "(import list [append]) (append [] [1 2 3])",
         .expected_str = "[1 2 3]",
         .expected_type = EXPECT_LIST},
        {.name = "append empty right",
         .src = "(import list [append]) (append [1 2 3] [])",
         .expected_str = "[1 2 3]",
         .expected_type = EXPECT_LIST},
        {.name = "append both empty",
         .src = "(import list [append]) (is_empty? (append [] []))",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
        {.name = "append preserves left list",
         .src = "(import list [append]) (let l [1 2]) (append l [3 4]) (len l)",
         .expected_str = "2",
         .expected_type = EXPECT_INT},
    };
    return run_list_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_list_map(void) {
    ListTestCase tests[] = {
        {.name = "map doubles each element",
         .src = "(import list [map]) (map (fn [x] (* x 2)) [1 2 3])",
         .expected_str = "[2 4 6]",
         .expected_type = EXPECT_LIST},
        {.name = "map over empty list returns empty",
         .src = "(import list [map]) (is_empty? (map (fn [x] x) []))",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
        {.name = "map preserves order",
         .src = "(import list [map]) (map (fn [x] (- 10 x)) [1 2 3 4])",
         .expected_str = "[9 8 7 6]",
         .expected_type = EXPECT_LIST},
        {.name = "map over singleton",
         .src = "(import list [map]) (map (fn [x] (* x x)) [5])",
         .expected_str = "[25]",
         .expected_type = EXPECT_LIST},
        {.name = "map with native function",
         .src = "(import list [map]) (import str [to_int]) (map to_int [\"1\" \"2\" \"3\"])",
         .expected_str = "[1 2 3]",
         .expected_type = EXPECT_LIST},
    };
    return run_list_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_list_reduce(void) {
    ListTestCase tests[] = {
        {.name = "reduce sum",
         .src = "(import list [reduce]) (reduce (fn [acc x] (+ acc x)) 0 [1 2 3 4 5])",
         .expected_str = "15",
         .expected_type = EXPECT_INT},
        {.name = "reduce product",
         .src = "(import list [reduce]) (reduce (fn [acc x] (* acc x)) 1 [1 2 3 4])",
         .expected_str = "24",
         .expected_type = EXPECT_INT},
        {.name = "reduce over empty list returns acc",
         .src = "(import list [reduce]) (reduce (fn [acc x] (+ acc x)) 99 [])",
         .expected_str = "99",
         .expected_type = EXPECT_INT},
        {.name = "reduce over singleton",
         .src = "(import list [reduce]) (reduce (fn [acc x] (+ acc x)) 10 [5])",
         .expected_str = "15",
         .expected_type = EXPECT_INT},
        {.name = "reduce builds list in reverse (right fold via cons)",
         .src = "(import list [reduce cons]) (reduce (fn [acc x] (cons acc x)) [] [1 2 3])",
         .expected_str = "[3 2 1]",
         .expected_type = EXPECT_LIST},
    };
    return run_list_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_list_composition(void) {
    ListTestCase tests[] = {
        {.name = "map then reduce",
         .src = "(import list [map reduce]) (reduce (fn [acc x] (+ acc x)) 0 (map (fn [x] (* x x)) [1 2 3]))",
         .expected_str = "14",
         .expected_type = EXPECT_INT},
        {.name = "recursive sum via head/tail",
         .src = "(import list [head tail])"
                "(fn sum [l] (cond (is_empty? l) 0 (+ (head l) (sum (tail l)))))"
                "(sum [1 2 3 4 5])",
         .expected_str = "15",
         .expected_type = EXPECT_INT},
        {.name = "cons then head recovers element",
         .src = "(import list [cons head]) (head (cons [2 3] 1))",
         .expected_str = "1",
         .expected_type = EXPECT_INT},
        {.name = "append then len",
         .src = "(import list [append]) (len (append [1 2 3] [4 5]))",
         .expected_str = "5",
         .expected_type = EXPECT_INT},
    };
    return run_list_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

static char *test_list_sort(void) {
    ListTestCase tests[] = {
        {.name = "sort integers",
         .src = "(import list [sort]) (sort [3 1 4 1 5 9 2 6])",
         .expected_str = "[1 1 2 3 4 5 6 9]",
         .expected_type = EXPECT_LIST},
        {.name = "sort already sorted",
         .src = "(import list [sort]) (sort [1 2 3])",
         .expected_str = "[1 2 3]",
         .expected_type = EXPECT_LIST},
        {.name = "sort reverse order",
         .src = "(import list [sort]) (sort [3 2 1])",
         .expected_str = "[1 2 3]",
         .expected_type = EXPECT_LIST},
        {.name = "sort singleton",
         .src = "(import list [sort]) (sort [42])",
         .expected_str = "[42]",
         .expected_type = EXPECT_LIST},
        {.name = "sort empty list",
         .src = "(import list [sort]) (is_empty? (sort []))",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
        {.name = "sort preserves duplicates",
         .src = "(import list [sort]) (sort [2 2 2])",
         .expected_str = "[2 2 2]",
         .expected_type = EXPECT_LIST},
        {.name = "sort_by ascending",
         .src = "(import list [sort_by]) (sort_by [3 1 2] (fn [a b] (< a b)))",
         .expected_str = "[1 2 3]",
         .expected_type = EXPECT_LIST},
        {.name = "sort_by descending",
         .src = "(import list [sort_by]) (sort_by [1 3 2] (fn [a b] (> a b)))",
         .expected_str = "[3 2 1]",
         .expected_type = EXPECT_LIST},
        {.name = "sort_by on empty list",
         .src = "(import list [sort_by]) (is_empty? (sort_by [] (fn [a b] (< a b))))",
         .expected_str = "true",
         .expected_type = EXPECT_BOOL},
    };
    return run_list_tests(tests, sizeof(tests) / sizeof(tests[0]));
}

void modules_list_suite(void) {
    printf("--- List Module Suite ---\n");
    mu_run_test(test_list_head_tail_last);
    mu_run_test(test_list_cons_push);
    mu_run_test(test_list_append);
    mu_run_test(test_list_map);
    mu_run_test(test_list_reduce);
    mu_run_test(test_list_composition);
    mu_run_test(test_list_sort);
}
