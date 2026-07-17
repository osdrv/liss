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
  const bool debug;
} CoreTestCase;

static char *test_core_containers(void) {
  CoreTestCase tests[] = {
      {.name = "dotted pair",
       .src = "(1 . 2)",
       .expected_str = "(1 . 2)",
       .expected_type = EXPECT_PAIR},
      {.name = "negative pair",
       .src = "(-1 . -5)",
       .expected_str = "(-1 . -5)",
       .expected_type = EXPECT_PAIR},
      {.name = "empty dict",
       .src = "(dict)",
       .expected_str = "(dict)",
       .expected_type = EXPECT_DICT},
      {.name = "dict with pairs",
       .src = "(dict (\"a\" . 1) (\"b\" . 2))",
       .expected_str = "(dict (\"a\" . 1) (\"b\" . 2))",
       .expected_type = EXPECT_DICT},
      {.name = "get list",
       .src = "(get [10 20] 1)",
       .expected_str = "20",
       .expected_type = EXPECT_INT},
      {.name = "get dict",
       .src = "(get (dict (\"foo\" . 42)) \"foo\")",
       .expected_str = "42",
       .expected_type = EXPECT_INT},
      {.name = "get string",
       .src = "(get \"abc\" 0)",
       .expected_str = "\"a\"",
       .expected_type = EXPECT_STRING},
      {.name = "dict put",
       .src = "(let d (put (dict) \"k\" 7)) (get d \"k\")",
       .expected_str = "7",
       .expected_type = EXPECT_INT},
      {.name = "dict has? true",
       .src = "(has? (dict (1 . 1)) 1)",
       .expected_str = "true",
       .expected_type = EXPECT_BOOL},
      {.name = "dict has? false",
       .src = "(has? (dict (1 . 1)) 2)",
       .expected_str = "false",
       .expected_type = EXPECT_BOOL},
      {.name = "dict del",
       .src = "(let d (del (dict (1 . 1)) 1)) (has? d 1)",
       .expected_str = "false",
       .expected_type = EXPECT_BOOL},
      {.name = "dict len",
       .src = "(len (dict (1 . 1) (2 . 2)))",
       .expected_str = "2",
       .expected_type = EXPECT_INT},
      {.name = "dict keys",
       .src = "(let d (dict (1 . \"a\") (2 . \"b\"))) (let k (keys d)) (len k)",
       .expected_str = "2",
       .expected_type = EXPECT_INT},
      {.name = "dict values",
       .src =
           "(let d (dict (1 . \"a\") (2 . \"b\"))) (let v (values d)) (len v)",
       .expected_str = "2",
       .expected_type = EXPECT_INT},
  };

  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {

    DEBUG_LOG("Running test_containers test: %s", tests[i].name);

    VMOptions options = defaultVMOptions();
    options.stress_gc = true;
    VM *vm = newVM(options);

    if (tests[i].debug) {
        DEBUG_LOG("Set a breakpoint for a failing test");
        __builtin_debugtrap();
    }

    InterpretResult result = interpret(vm, tests[i].src, NULL);
    if (result != INTERPRET_OK) {
      printf("Failed test: %s (InterpretResult: %d)\n", tests[i].name, result);
      mu_assert("Interpretation failed", false);
    }

    Value val = vm->last_popped_value;
    char *assert_msg = NULL;

    switch (tests[i].expected_type) {
    case EXPECT_PAIR:
      assert_msg = assert_pair(val, tests[i].expected_str);
      break;
    case EXPECT_DICT:
      assert_msg = assert_dict(val, tests[i].expected_str);
      break;
    case EXPECT_INT:
      assert_msg = assert_int(val, atoll(tests[i].expected_str));
      break;
    case EXPECT_BOOL:
      assert_msg = assert_bool(val, strcmp(tests[i].expected_str, "true") == 0);
      break;
    case EXPECT_STRING: {
      mu_assert("Value is not string", IS_STRING(val));
      char *s = sprintValue(val);
      mu_assert("String mismatch", strcmp(s, tests[i].expected_str) == 0);
      free(s);
    } break;
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

static char *test_core_lists(void) {
  CoreTestCase tests[] = {
      // head
      {.name = "head of list",
       .src = "(head [1 2 3])",
       .expected_str = "1",
       .expected_type = EXPECT_INT},
      {.name = "head of single-element list",
       .src = "(head [42])",
       .expected_str = "42",
       .expected_type = EXPECT_INT},
      // tail
      {.name = "tail of list",
       .src = "(tail [1 2 3])",
       .expected_str = "[2 3]",
       .expected_type = EXPECT_LIST},
      {.name = "tail of single-element list is empty",
       .src = "(tail [42])",
       .expected_str = "[]",
       .expected_type = EXPECT_LIST},
      // cons
      {.name = "cons prepends element",
       .src = "(cons [1 2 3] 0)",
       .expected_str = "[0 1 2 3]",
       .expected_type = EXPECT_LIST},
      {.name = "cons onto empty list",
       .src = "(cons [] 99)",
       .expected_str = "[99]",
       .expected_type = EXPECT_LIST},
      {.name = "cons preserves original",
       .src = "(let lst [1 2]) (cons lst 0) (len lst)",
       .expected_str = "2",
       .expected_type = EXPECT_INT},
      // push
      {.name = "push appends element",
       .src = "(push [1 2 3] 4)",
       .expected_str = "[1 2 3 4]",
       .expected_type = EXPECT_LIST},
      {.name = "push onto empty list",
       .src = "(push [] 99)",
       .expected_str = "[99]",
       .expected_type = EXPECT_LIST},
      {.name = "push preserves original",
       .src = "(let lst [1 2]) (push lst 3) (len lst)",
       .expected_str = "2",
       .expected_type = EXPECT_INT},
      // append
      {.name = "append two lists",
       .src = "(append [1 2] [3 4])",
       .expected_str = "[1 2 3 4]",
       .expected_type = EXPECT_LIST},
      {.name = "append empty left",
       .src = "(append [] [1 2])",
       .expected_str = "[1 2]",
       .expected_type = EXPECT_LIST},
      {.name = "append empty right",
       .src = "(append [1 2] [])",
       .expected_str = "[1 2]",
       .expected_type = EXPECT_LIST},
      {.name = "append preserves left original",
       .src = "(let a [1 2]) (append a [3 4]) (len a)",
       .expected_str = "2",
       .expected_type = EXPECT_INT},
      // combinations
      {.name = "head after cons",
       .src = "(head (cons [1 2] 99))",
       .expected_str = "99",
       .expected_type = EXPECT_INT},
      {.name = "tail after push",
       .src = "(tail (push [1 2 3] 4))",
       .expected_str = "[2 3 4]",
       .expected_type = EXPECT_LIST},
      {.name = "len after append",
       .src = "(len (append [1 2 3] [4 5]))",
       .expected_str = "5",
       .expected_type = EXPECT_INT},
  };

  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    DEBUG_LOG("Running test_core_lists test: %s", tests[i].name);

    VMOptions options = defaultVMOptions();
    options.stress_gc = true;
    VM *vm = newVM(options);

    if (tests[i].debug) {
        DEBUG_LOG("Set a breakpoint for a failing test");
        __builtin_debugtrap();
    }

    InterpretResult result = interpret(vm, tests[i].src, NULL);
    if (result != INTERPRET_OK) {
      printf("Failed test: %s (InterpretResult: %d)\n", tests[i].name, result);
      mu_assert("Interpretation failed", false);
    }

    Value val = vm->last_popped_value;
    char *assert_msg = NULL;

    switch (tests[i].expected_type) {
    case EXPECT_LIST:
      assert_msg = assert_list(val, tests[i].expected_str);
      break;
    case EXPECT_INT:
      assert_msg = assert_int(val, atoll(tests[i].expected_str));
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

void modules_core_suite(void) {
  printf("--- Core Module Suite ---\n");
  mu_run_test(test_core_containers);
  mu_run_test(test_core_lists);
}
