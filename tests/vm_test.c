#include "vm.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "minunit.h"
#include "test_common.h"
#include "value.h"

typedef struct {
    const char* name;
    const char* src;
    InterpretResult expected_result;
    ExpectedValue expected_value;
    bool debug;
} VMTestCase;

static char* test_vm_stack(void) {
    VMOptions options = {
        .stack_capacity = 16,
        .gc_threshold = 1024,
        .heap_growth_factor = 2,
        .stress_gc = true,
        .frames_max = 32,
    };
    VM* vm = newVM(options);  // Create a VM with a small stack for testing
    mu_assert("Failed to create VM", vm != NULL);

    // Test push and pop
    push(vm, INT_VAL(42));
    Value v = pop(vm);

    mu_assert("Popped value should be an integer", IS_INT(v));
    mu_assert("Popped value should be 42", AS_INT(v) == 42);

    destroyVM(vm);
    return NULL;
}

static VMTestCase interpret_tests[] = {
    {
        .name = "literal number",
        .src = "123",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 123},
    },
    {
        .name = "simple addition",
        .src = "(+ 1 2)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 3},
    },
    {
        .name = "int + real",
        .src = "(+ 1 2.0)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_REAL, .as.real = 3.0},
    },
    {
        .name = "real + int",
        .src = "(+ 1.5 2)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_REAL, .as.real = 3.5},
    },
    {
        .name = "real + real",
        .src = "(+ 1.5 2.5)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_REAL, .as.real = 4.0},
    },
    {
        .name = "int * int",
        .src = "(* 3 4)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 12},
    },
    {
        .name = "int * real",
        .src = "(* 3 2.5)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_REAL, .as.real = 7.5},
    },
    {
        .name = "real * int",
        .src = "(* 2.5 3)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_REAL, .as.real = 7.5},
    },
    {
        .name = "real * real",
        .src = "(* 2.0 2.0)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_REAL, .as.real = 4.0},
    },
    {
        .name = "int / int",
        .src = "(/ 9 2)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 4},
    },
    {
        .name = "int / real",
        .src = "(/ 9 2.0)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_REAL, .as.real = 4.5},
    },
    {
        .name = "real / int",
        .src = "(/ 9.0 2)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_REAL, .as.real = 4.5},
    },
    {
        .name = "real / real",
        .src = "(/ 9.0 2.0)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_REAL, .as.real = 4.5},
    },
    {
        .name = "nested expression",
        .src = "(- (+ 10 5) 3)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 12},
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
        .name = "BAND kw expression",
        .src = "(band 3 7)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 3},
    },
    {
        .name = "BAND op expression",
        .src = "(&& 3 7)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 3},
    },
    {
        .name = "BOR kw expression",
        .src = "(bor 1 2)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 3},
    },
    {
        .name = "BOR op expression",
        .src = "(|| 1 2)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 3},
    },
    {
        .name = "BXOR kw expression",
        .src = "(bxor 1 3)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 2},
    },
    {
        .name = "BXOR op expression",
        .src = "(^ 1 3)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 2},
    },
    {
        .name = "BNOT kw expression",
        .src = "(bnot 1)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = ~1},
    },
    {
        .name = "BNOT op expression",
        .src = "(~ 1)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = ~1},
    },
    {
        .name = "LSHIFT kw expression",
        .src = "(bsl 1 3)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 8},
    },
    {
        .name = "LSHIFT op expression",
        .src = "(<< 1 3)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 8},
    },
    {
        .name = "RSHIFT kw expression",
        .src = "(bsr 8 3)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 1},
    },
    {
        .name = "RSHIFT op expression",
        .src = "(>> 8 3)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 1},
    },
    {
        .name = "cond expression with else branch",
        .src = "(cond false 123 456)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 456},
    },
    {
        .name = "cond expression with then branch taken",
        .src = "(cond true 123 456)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 123},
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
        .expected_value = {EXPECT_INT, .as.integer = 123},
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
        .expected_value = {EXPECT_INT, .as.integer = 10},
    },
    {
        .name = "let expression with arithmetic",
        .src = "(let y (+ 5 5))",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 10},
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
        .expected_value = {EXPECT_INT, .as.integer = 43},
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
        .expected_value = {EXPECT_INT, .as.integer = 123},
    },
    {
        .name = "function call with parameters",
        .src = "(fn add [a b] (+ a b)) (add 10 20)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 30},
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
        .expected_value = {EXPECT_INT, .as.integer = 3},
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
        .expected_value = {EXPECT_INT, .as.integer = 30},
    },
    {
        .name = "lambda closure",
        .src = "(fn mk_multiplier [x]"
               "         (fn [y] (* x y))"
               ")"
               "(let double (mk_multiplier 2))"
               "(double 5)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 10},
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
    {
        .name = "unhandled raise! should cause a runtime error",
        .src = "(raise! \"did you miss me?\")",
        .expected_result = INTERPRET_RUNTIME_ERROR,
    },
    {
        .name = "try expression with a scalar and no exception",
        .src = "(try 123)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 123},
    },
    {
        .name = "try expression with an expression with no exception",
        .src = "(try (+ 123 456))",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 579},
    },
    {
        .name = "try expression with handled exception",
        .src = "(try (raise! (err \"oops\")))",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_ERROR, .as.string = "oops"},
    },
    {
        .name = "empty list expression",
        .src = "[]",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_LIST, .as.string = "[]"},
    },
    {
        .name = "list with ints",
        .src = "[1 2 3]",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_LIST, .as.string = "[1 2 3]"},
    },
    {
        .name = "list with mixed types",
        .src = "[1 true null \"hello\"]",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_LIST, .as.string = "[1 true null \"hello\"]"},
    },
    {
        .name = "pair with mixed types",
        .src = "(\"foo\" . 42)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_PAIR, .as.string = "(\"foo\" . 42)"},
    },
    {
        .name = "native fn call",
        .src = "(err \"test error\")",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_ERROR, .as.string = "test error"},
    },
    {
        .name = "native fn call with a core-prefixed FQN",
        .src = "(core:err \"test error\")",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_ERROR, .as.string = "test error"},
    },
    {
        .name = "plain import as identifier",
        .src = "(import core)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "plain import as stringified name",
        .src = "(import \"core\")",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "aliased import",
        .src = "(import core as c)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "import with empty symbol list",
        .src = "(import core [])",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "import with non-empty symbol list",
        .src = "(import core [\"err\"])",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "aliased import with non-empty symbol list",
        .src = "(import core as c [\"err\"])",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_BOOL, .as.boolean = true},
    },
    {
        .name = "native fn call with an imported core-prefixed FQN",
        .src = "(import core) (core:err \"test error\")",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_ERROR, .as.string = "test error"},
    },
    {
        .name = "native fn call with an aliased imported core-prefixed FQN",
        .src = "(import core as c) (c:err \"test error\")",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_ERROR, .as.string = "test error"},
    },
    {
        .name = "import optional module and call a function",
        .src = "(import math)(math:floor 1.23)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_REAL, .as.real = 1.0},
    },
    {
        .name = "empty dict",
        .src = "(dict)",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_DICT, .as.string = "(dict)"},
    },
    {
        .name = "a dict with string keys",
        .src = "(dict (\"foo\" . 1) (\"bar\" . 2) (\"boo\" . 3))",
        .expected_result = INTERPRET_OK,
        .expected_value =
            {EXPECT_DICT,
             .as.string = "(dict (\"bar\" . 2) (\"boo\" . 3) (\"foo\" . 1))"},
    },
    {
        .name = "a dict with various keys",
        .src = "(dict (1 . 2) (3.14 . 15.16) (-1 . -1) (true . false) (\"foo\" "
               ". \"bar\"))",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_DICT,
                           .as.string =
                               "(dict (true . false) (-1 . -1) (1 . 2) (3.14 . "
                               "15.16) (\"foo\" . \"bar\"))"},
    },
    {
        .name = "regex match with capturing groups",
        .src = "(import re)(re:match (re:re \"a(b*)(c+)\") \"abbbc\")",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_LIST,
                           .as.string = "[\"abbbc\" \"bbb\" \"c\"]"},
    },
    {
        .name = "regex match non-matching optional group",
        .src = "(import re)(re:match (re:re \"a(b)?c\") \"ac\")",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_LIST, .as.string = "[\"ac\" null]"},
    },
    {
        .name = "regex match failing match returns nil",
        .src = "(import re)(re:match (re:re \"a(b)c\") \"adc\")",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_NIL},
    },
    {
        .name = "switch with literal match",
        .src = "(switch 2"
               "[1 \"one\"]"
               "[2 \"two\"]"
               "[* \"many\"])",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_STRING, .as.string = "two"},
    },
    {
        .name = "switch with literal wildcard match",
        .src = "(switch 3"
               "[1 \"one\"]"
               "[2 \"two\"]"
               "[* \"many\"])",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_STRING, .as.string = "many"},
    },
    {
        .name = "switch with deconstructing err match",
        .src = "(switch (err \"oops\")"
               "[(err msg) msg]"
               "[* \"ok\"])",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_STRING, .as.string = "oops"},
    },
    {
        .name = "switch with deconstructing err wildcard match",
        .src = "(switch null"
               "[(err msg) msg]"
               "[* \"ok\"])",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_STRING, .as.string = "ok"},
    },
    {
        .name = "switch with deconstructing pair match",
        .src = "(switch (1 . 2)"
               "[(pair a b) (+ a b)]"
               "[* 0])",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 3},
    },
    {
        .name = "switch with deconstructing pair wildcard match",
        .src = "(switch null"
               "[(pair a b) (+ a b)]"
               "[* 0])",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_INT, .as.integer = 0},
    },
    {
        .name = "pipe single step",
        .src = "(import str)(-> \"  hello  \" (str:trim))",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_STRING, .as.string = "hello"},
    },
    {
        .name = "pipe multi step",
        .src =
            "(import str)(-> \"  hello:world  \" (str:trim) (str:split \":\"))",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_LIST, .as.string = "[\"hello\" \"world\"]"},
    },
    {
        .name = "pipe short-circuits on err",
        .src = "(import str)(-> (err \"bad\") (str:trim))",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_ERROR, .as.string = "bad"},
    },
    {
        .name = "pipe step returning err short-circuits remaining steps",
        .src = "(import str)(-> \"bad\" (err) (str:trim))",
        .expected_result = INTERPRET_OK,
        .expected_value = {EXPECT_ERROR, .as.string = "bad"},
    },
};

static char* test_vm_interpret(void) {
    for (size_t i = 0; i < sizeof(interpret_tests) / sizeof(interpret_tests[0]);
         i++) {
        VMTestCase* test = &interpret_tests[i];
        DEBUG_LOG("Running vm test: %s", test->name);
        VMOptions options = {
            .stack_capacity = 64,
            .gc_threshold = 1024,
            .heap_growth_factor = 2,
            .stress_gc = true,
            .frames_max = 32,
        };
        VM* vm = newVM(options);
        mu_assert("Failed to create VM", vm != NULL);

        if (test->debug) {
            DEBUG_LOG("Set a breakpoint for a failing test");
            __builtin_debugtrap();
        }

        InterpretResult result = interpret(vm, test->src, NULL);
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
                case EXPECT_PAIR:
                    assert_msg = assert_pair(actual, expected.as.string);
                    break;
                case EXPECT_DICT:
                    assert_msg = assert_dict(actual, expected.as.string);
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
    }

    return NULL;
}

// The suite function, called by the main test runner.
void vm_suite(void) {
    printf("--- VM Suite ---\n");
    mu_run_test(test_vm_stack);
    mu_run_test(test_vm_interpret);
}
