#include "compiler.h"

#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "minunit.h"
#include "object.h"
#include "opcode.h"
#include "value.h"
#include "vm.h"

typedef struct {
    const char* name;
    const char* src;
    uint8_t* expected_instructions;
    size_t expected_instruction_count;
    Value* expected_constants;
    size_t expected_constant_count;
} CompilerTestCase;

static char* assert_instructions(Chunk* chunk, uint8_t expected[],
                                 size_t expected_count) {
    mu_assert("Chunk instruction count does not match expected count.",
              chunk->count == (int)expected_count);
    for (size_t i = 0; i < expected_count; i++) {
        if (chunk->code[i] != expected[i]) {
            DEBUG_LOG("At instruction %zu: expected %d but got %d", i,
                      expected[i], chunk->code[i]);
            printChunk(chunk);
        }
        mu_assert("Chunk instruction does not match expected instruction.",
                  chunk->code[i] == expected[i]);
    }

    return NULL;
}

static char* assert_values_equal(Value* a, Value* b) {
    mu_assert("Values are not equal", valuesEqual(*a, *b));
    return NULL;
}

static char* assert_constants(Chunk* chunk, Value expected[],
                              size_t expected_count) {
    mu_assert("Chunk constant count does not match expected count.",
              chunk->constants.count == (int)expected_count);
    for (size_t i = 0; i < expected_count; i++) {
        Value val = chunk->constants.values[i];
        if (assert_values_equal(&val, &expected[i]) != NULL) {
            DEBUG_LOG("At constant %zu: expected different value.", i);
        }
        mu_assert("Values must be equal.",
                  assert_values_equal(&val, &expected[i]) == NULL);
    }

    return NULL;
}

static char* test_compile(void) {
    CompilerTestCase compile_tests[] = {
        {
            .name = "compile simple number",
            .src = "123",
            .expected_instructions = (uint8_t[]){OP_CONSTANT, 0, 0, OP_RETURN},
            .expected_instruction_count = 4,
            .expected_constants = (Value[]){NUMBER_VAL(123.0)},
            .expected_constant_count = 1,
        },
        {
            .name = "compile simple addition",
            .src = "(+ 1 2)",
            .expected_instructions = (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT,
                                                 0, 1, OP_ADD, OP_RETURN},
            .expected_instruction_count = 8,
            .expected_constants = (Value[]){NUMBER_VAL(1.0), NUMBER_VAL(2.0)},
            .expected_constant_count = 2,
        },
        {
            .name = "compile nested expression",
            .src = "(- (+ 10 5) 3)",
            .expected_instructions =
                (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT, 0, 1, OP_ADD,
                            OP_CONSTANT, 0, 2, OP_SUBTRACT, OP_RETURN},
            .expected_instruction_count = 12,
            .expected_constants =
                (Value[]){NUMBER_VAL(10.0), NUMBER_VAL(5.0), NUMBER_VAL(3.0)},
            .expected_constant_count = 3,
        },
        {
            .name = "compile boolean literal true",
            .src = "true",
            .expected_instructions = (uint8_t[]){OP_TRUE, OP_RETURN},
            .expected_instruction_count = 2,
            .expected_constants = NULL,
            .expected_constant_count = 0,
        },
        {
            .name = "compile boolean literal false",
            .src = "false",
            .expected_instructions = (uint8_t[]){OP_FALSE, OP_RETURN},
            .expected_instruction_count = 2,
            .expected_constants = NULL,
            .expected_constant_count = 0,
        },
        {
            .name = "compile null literal",
            .src = "null",
            .expected_instructions = (uint8_t[]){OP_NULL, OP_RETURN},
            .expected_instruction_count = 2,
            .expected_constants = NULL,
            .expected_constant_count = 0,
        },
        {
            .name = "compile not operation",
            .src = "(! true)",
            .expected_instructions = (uint8_t[]){OP_TRUE, OP_NOT, OP_RETURN},
            .expected_instruction_count = 3,
            .expected_constants = NULL,
            .expected_constant_count = 0,
        },
        {
            .name = "compile AND expression with 2 operands",
            .src = "(and true false)",
            .expected_instructions = (uint8_t[]){OP_TRUE, OP_JUMP_IF_FALSE, 0,
                                                 1, OP_FALSE, OP_RETURN},
            .expected_instruction_count = 6,
            .expected_constants = NULL,
            .expected_constant_count = 0,
        },
        {
            .name = "compile AND expression with more operands",
            .src = "(and true true false)",
            .expected_instructions =
                (uint8_t[]){OP_TRUE, OP_JUMP_IF_FALSE, 0, 5, OP_TRUE,
                            OP_JUMP_IF_FALSE, 0, 1, OP_FALSE, OP_RETURN},
            .expected_instruction_count = 10,
            .expected_constants = NULL,
            .expected_constant_count = 0,
        },
        {
            .name = "compile AND with all true",
            .src = "(and true true true)",
            .expected_instructions =
                (uint8_t[]){OP_TRUE, OP_JUMP_IF_FALSE, 0, 5, OP_TRUE,
                            OP_JUMP_IF_FALSE, 0, 1, OP_TRUE, OP_RETURN},
            .expected_instruction_count = 10,
            .expected_constants = NULL,
            .expected_constant_count = 0,
        },
        {
            .name = "compile AND with all false",
            .src = "(and false false false)",
            .expected_instructions =
                (uint8_t[]){OP_FALSE, OP_JUMP_IF_FALSE, 0, 5, OP_FALSE,
                            OP_JUMP_IF_FALSE, 0, 1, OP_FALSE, OP_RETURN},
            .expected_instruction_count = 10,
            .expected_constants = NULL,
            .expected_constant_count = 0,
        },
        {
            .name = "compile OR expression with 2 operands",
            .src = "(or false true)",
            .expected_instructions =
                (uint8_t[]){OP_FALSE, OP_JUMP_IF_FALSE, 0, 3, OP_JUMP, 0, 2,
                            OP_POP, OP_TRUE, OP_RETURN},
            .expected_instruction_count = 10,
            .expected_constants = NULL,
            .expected_constant_count = 0,
        },
        {
            .name = "compile OR expression with more operands",
            .src = "(or false false true false)",
            .expected_instructions =
                (uint8_t[]){
                    OP_FALSE, OP_JUMP_IF_FALSE, 0, 3, OP_JUMP, 0, 18, OP_POP,
                    OP_FALSE, OP_JUMP_IF_FALSE, 0, 3, OP_JUMP, 0, 10, OP_POP,
                    OP_TRUE,  OP_JUMP_IF_FALSE, 0, 3, OP_JUMP, 0, 2,  OP_POP,
                    OP_FALSE, OP_RETURN},
            .expected_instruction_count = 26,
            .expected_constants = NULL,
            .expected_constant_count = 0,
        },
        {
            .name = "compile cond expression with no else branch",
            .src = "(cond true 123)",
            .expected_instructions =
                (uint8_t[]){OP_TRUE, OP_JUMP_IF_FALSE, 0, 7, OP_POP,
                            OP_CONSTANT, 0, 0, OP_JUMP, 0, 2, OP_POP, OP_NULL,
                            OP_RETURN},
            .expected_instruction_count = 14,
            .expected_constants = (Value[]){NUMBER_VAL(123.0)},
            .expected_constant_count = 1,
        },
        {
            .name =
                "compile cond expression with no else resolving to else branch",
            .src = "(cond false 123)",
            .expected_instructions =
                (uint8_t[]){OP_FALSE, OP_JUMP_IF_FALSE, 0, 7, OP_POP,
                            OP_CONSTANT, 0, 0, OP_JUMP, 0, 2, OP_POP, OP_NULL,
                            OP_RETURN},
            .expected_instruction_count = 14,
            .expected_constants = (Value[]){NUMBER_VAL(123.0)},
            .expected_constant_count = 1,
        },
        {
            .name = "compile cond expression with else branch",
            .src = "(cond false 123 456)",
            .expected_instructions =
                (uint8_t[]){OP_FALSE, OP_JUMP_IF_FALSE, 0, 7, OP_POP,
                            OP_CONSTANT, 0, 0, OP_JUMP, 0, 4, OP_POP,
                            OP_CONSTANT, 0, 1, OP_RETURN},
            .expected_instruction_count = 16,
            .expected_constants =
                (Value[]){NUMBER_VAL(123.0), NUMBER_VAL(456.0)},
            .expected_constant_count = 2,
        },
        {
            .name = "compile compare operation: = operator",
            .src = "(= 1 1)",
            .expected_instructions = (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT,
                                                 0, 1, OP_EQUAL, OP_RETURN},
            .expected_instruction_count = 8,
            .expected_constants = (Value[]){NUMBER_VAL(1.0), NUMBER_VAL(1.0)},
            .expected_constant_count = 2,
        },
        {
            .name = "compile compare operation: != operator",
            .src = "(!= 1 2)",
            .expected_instructions =
                (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT, 0, 1, OP_EQUAL,
                            OP_NOT, OP_RETURN},
            .expected_instruction_count = 9,
            .expected_constants = (Value[]){NUMBER_VAL(1.0), NUMBER_VAL(2.0)},
            .expected_constant_count = 2,
        },
        {
            .name = "compile compare operation: > operator",
            .src = "(> 2 1)",
            .expected_instructions = (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT,
                                                 0, 1, OP_GREATER, OP_RETURN},
            .expected_instruction_count = 8,
            .expected_constants = (Value[]){NUMBER_VAL(2.0), NUMBER_VAL(1.0)},
            .expected_constant_count = 2,
        },
        {
            .name = "compile compare operation: < operator",
            .src = "(< 1 2)",
            .expected_instructions = (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT,
                                                 0, 1, OP_LESS, OP_RETURN},
            .expected_instruction_count = 8,
            .expected_constants = (Value[]){NUMBER_VAL(1.0), NUMBER_VAL(2.0)},
            .expected_constant_count = 2,
        },
        {
            .name = "compile compare operation: <= operator",
            .src = "(<= 1 2)",
            .expected_instructions =
                (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT, 0, 1, OP_GREATER,
                            OP_NOT, OP_RETURN},
            .expected_instruction_count = 9,
            .expected_constants = (Value[]){NUMBER_VAL(1.0), NUMBER_VAL(2.0)},
            .expected_constant_count = 2,
        },
        {
            .name = "compile compare operation: >= operator",
            .src = "(>= 2 1)",
            .expected_instructions =
                (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT, 0, 1, OP_LESS,
                            OP_NOT, OP_RETURN},
            .expected_instruction_count = 9,
            .expected_constants = (Value[]){NUMBER_VAL(2.0), NUMBER_VAL(1.0)},
            .expected_constant_count = 2,
        },
        {
            .name = "compile let expression",
            .src = "(let x 42)",
            .expected_instructions =
                (uint8_t[]){OP_CONSTANT, 0, 1, OP_SET_GLOBAL, 0, 0, OP_RETURN},
            .expected_instruction_count = 7,
        },
    };

    for (size_t i = 0; i < sizeof(compile_tests) / sizeof(compile_tests[0]);
         i++) {
        CompilerTestCase* test = &compile_tests[i];
        VM* vm = newVM(256);

        ObjFunction* function = compile(vm, test->src);
        mu_assert("Compiler should not fail.", function != NULL);

        Chunk* chunk = &function->chunk;

        if (chunk->count != (int)test->expected_instruction_count) {
            DEBUG_LOG("Unexpected instruction count");
            printChunk(chunk);
        }
        DEBUG_LOG("Running test: %s", test->name);
        mu_assert("Instruction count should match expected count.",
                  chunk->count == (int)test->expected_instruction_count);
        mu_assert(
            "Instructions should match expected instructions.",
            assert_instructions(chunk, test->expected_instructions,
                                test->expected_instruction_count) == NULL);

        if (strcmp(test->name, "compile let expression") == 0) {
            mu_assert("Constant count should be 2.",
                      chunk->constants.count == 2);
            Value first_const = chunk->constants.values[0];
            mu_assert("First constant should be a string.",
                      IS_OBJ(first_const) && IS_STRING(first_const));
            ObjString* str = AS_STRING(first_const);
            mu_assert("String should be 'x'", strcmp(str->chars, "x") == 0);
            Value second_const = chunk->constants.values[1];
            mu_assert("Second constant should be a number.",
                      IS_NUMBER(second_const));
            mu_assert("Number should be 42.0", AS_NUMBER(second_const) == 42.0);
        } else {
            mu_assert(
                "%s: Constant count should match expected count.",
                chunk->constants.count == (int)test->expected_constant_count);
            mu_assert("%s: Constants should match expected constants.",
                      assert_constants(chunk, test->expected_constants,
                                       test->expected_constant_count) == NULL);
        }

        destroyVM(vm);
    }

    return NULL;
}

void compiler_suite(void) {
    printf("--- Compiler Suite ---\n");
    mu_run_test(test_compile);
}
