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

typedef enum {
    EXPECT_NUMBER,
    EXPECT_OBJ_STRING,
} ExpectedConstantType;

typedef struct {
    ExpectedConstantType type;
    union {
        double number;
        const char* obj_string;
    } as;
} ExpectedConstant;

typedef struct {
    const char* name;
    const char* src;
    uint8_t* expected_instructions;
    size_t expected_instruction_count;
    ExpectedConstant* expected_constants;
    size_t expected_constant_size;
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

static char* assert_number(Value value, double expected) {
    mu_assert("Value is not a number.", IS_NUMBER(value));
    mu_assert("Number value does not match expected.",
              AS_NUMBER(value) == expected);
    return NULL;
}

static char* assert_string(Value value, const char* expected) {
    mu_assert("Value is not an object.", IS_OBJ(value));
    mu_assert("Object is not a string.", OBJ_TYPE(value) == OBJ_STRING);
    ObjString* str = AS_STRING(value);
    mu_assert("String length does not match expected.",
              str->length == (int)strlen(expected));
    mu_assert("String contents do not match expected.",
              memcmp(str->chars, expected, str->length) == 0);
    return NULL;
}

static char* assert_constants(Chunk* chunk, ExpectedConstant expected[],
                              size_t expected_count) {
    mu_assert("Chunk constant count does not match expected count.",
              chunk->constants.count == (int)expected_count);
    for (size_t i = 0; i < expected_count; i++) {
        Value actual = chunk->constants.values[i];
        ExpectedConstant exp = expected[i];
        switch (exp.type) {
            case EXPECT_NUMBER:
                mu_assert("Constant verification failed for number.",
                          assert_number(actual, exp.as.number) == NULL);
                break;
            case EXPECT_OBJ_STRING:
                mu_assert("Constant verification failed for string.",
                          assert_string(actual, exp.as.obj_string) == NULL);
                break;
            default:
                mu_assert("Unknown expected constant type.", false);
        }
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
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_NUMBER, .as.number = 123.0},
                },
            .expected_constant_size = 1,
        },
        {
            .name = "compile simple string",
            .src = "\"hello\"",
            .expected_instructions = (uint8_t[]){OP_CONSTANT, 0, 0, OP_RETURN},
            .expected_instruction_count = 4,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_OBJ_STRING, .as.obj_string = "hello"},
                },
            .expected_constant_size = 1,
        },
        {
            .name = "compile simple addition",
            .src = "(+ 1 2)",
            .expected_instructions = (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT,
                                                 0, 1, OP_ADD, OP_RETURN},
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_NUMBER, .as.number = 1.0},
                    {EXPECT_NUMBER, .as.number = 2.0},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile nested expression",
            .src = "(- (+ 10 5) 3)",
            .expected_instructions =
                (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT, 0, 1, OP_ADD,
                            OP_CONSTANT, 0, 2, OP_SUBTRACT, OP_RETURN},
            .expected_instruction_count = 12,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_NUMBER, .as.number = 10.0},
                    {EXPECT_NUMBER, .as.number = 5.0},
                    {EXPECT_NUMBER, .as.number = 3.0},
                },
            .expected_constant_size = 3,
        },
        {
            .name = "compile boolean literal true",
            .src = "true",
            .expected_instructions = (uint8_t[]){OP_TRUE, OP_RETURN},
            .expected_instruction_count = 2,
            .expected_constants = NULL,
            .expected_constant_size = 0,
        },
        {
            .name = "compile boolean literal false",
            .src = "false",
            .expected_instructions = (uint8_t[]){OP_FALSE, OP_RETURN},
            .expected_instruction_count = 2,
            .expected_constants = NULL,
            .expected_constant_size = 0,
        },
        {
            .name = "compile null literal",
            .src = "null",
            .expected_instructions = (uint8_t[]){OP_NULL, OP_RETURN},
            .expected_instruction_count = 2,
            .expected_constants = NULL,
            .expected_constant_size = 0,
        },
        {
            .name = "compile not operation",
            .src = "(! true)",
            .expected_instructions = (uint8_t[]){OP_TRUE, OP_NOT, OP_RETURN},
            .expected_instruction_count = 3,
            .expected_constants = NULL,
            .expected_constant_size = 0,
        },
        {
            .name = "compile AND expression with 2 operands",
            .src = "(and true false)",
            .expected_instructions = (uint8_t[]){OP_TRUE, OP_JUMP_IF_FALSE, 0,
                                                 1, OP_FALSE, OP_RETURN},
            .expected_instruction_count = 6,
            .expected_constants = NULL,
            .expected_constant_size = 0,
        },
        {
            .name = "compile AND expression with more operands",
            .src = "(and true true false)",
            .expected_instructions =
                (uint8_t[]){OP_TRUE, OP_JUMP_IF_FALSE, 0, 5, OP_TRUE,
                            OP_JUMP_IF_FALSE, 0, 1, OP_FALSE, OP_RETURN},
            .expected_instruction_count = 10,
            .expected_constants = NULL,
            .expected_constant_size = 0,
        },
        {
            .name = "compile AND with all true",
            .src = "(and true true true)",
            .expected_instructions =
                (uint8_t[]){OP_TRUE, OP_JUMP_IF_FALSE, 0, 5, OP_TRUE,
                            OP_JUMP_IF_FALSE, 0, 1, OP_TRUE, OP_RETURN},
            .expected_instruction_count = 10,
            .expected_constants = NULL,
            .expected_constant_size = 0,
        },
        {
            .name = "compile AND with all false",
            .src = "(and false false false)",
            .expected_instructions =
                (uint8_t[]){OP_FALSE, OP_JUMP_IF_FALSE, 0, 5, OP_FALSE,
                            OP_JUMP_IF_FALSE, 0, 1, OP_FALSE, OP_RETURN},
            .expected_instruction_count = 10,
            .expected_constants = NULL,
            .expected_constant_size = 0,
        },
        {
            .name = "compile OR expression with 2 operands",
            .src = "(or false true)",
            .expected_instructions =
                (uint8_t[]){OP_FALSE, OP_JUMP_IF_FALSE, 0, 3, OP_JUMP, 0, 2,
                            OP_POP, OP_TRUE, OP_RETURN},
            .expected_instruction_count = 10,
            .expected_constants = NULL,
            .expected_constant_size = 0,
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
            .expected_constant_size = 0,
        },
        {
            .name = "compile cond expression with no else branch",
            .src = "(cond true 123)",
            .expected_instructions =
                (uint8_t[]){OP_TRUE, OP_JUMP_IF_FALSE, 0, 7, OP_POP,
                            OP_CONSTANT, 0, 0, OP_JUMP, 0, 2, OP_POP, OP_NULL,
                            OP_RETURN},
            .expected_instruction_count = 14,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_NUMBER, .as.number = 123.0},
                },
            .expected_constant_size = 1,
        },
        {
            .name = "compile cond expression with no else resolving to else "
                    "branch",
            .src = "(cond false 123)",
            .expected_instructions =
                (uint8_t[]){OP_FALSE, OP_JUMP_IF_FALSE, 0, 7, OP_POP,
                            OP_CONSTANT, 0, 0, OP_JUMP, 0, 2, OP_POP, OP_NULL,
                            OP_RETURN},
            .expected_instruction_count = 14,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_NUMBER, .as.number = 123.0},
                },
            .expected_constant_size = 1,
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
                (ExpectedConstant[]){
                    {EXPECT_NUMBER, .as.number = 123.0},
                    {EXPECT_NUMBER, .as.number = 456.0},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile compare operation: = operator",
            .src = "(= 1 1)",
            .expected_instructions = (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT,
                                                 0, 1, OP_EQUAL, OP_RETURN},
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_NUMBER, .as.number = 1.0},
                    {EXPECT_NUMBER, .as.number = 1.0},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile compare operation: != operator",
            .src = "(!= 1 2)",
            .expected_instructions =
                (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT, 0, 1, OP_EQUAL,
                            OP_NOT, OP_RETURN},
            .expected_instruction_count = 9,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_NUMBER, .as.number = 1.0},
                    {EXPECT_NUMBER, .as.number = 2.0},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile compare operation: > operator",
            .src = "(> 2 1)",
            .expected_instructions = (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT,
                                                 0, 1, OP_GREATER, OP_RETURN},
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_NUMBER, .as.number = 2.0},
                    {EXPECT_NUMBER, .as.number = 1.0},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile compare operation: < operator",
            .src = "(< 1 2)",
            .expected_instructions = (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT,
                                                 0, 1, OP_LESS, OP_RETURN},
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_NUMBER, .as.number = 1.0},
                    {EXPECT_NUMBER, .as.number = 2.0},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile compare operation: <= operator",
            .src = "(<= 1 2)",
            .expected_instructions =
                (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT, 0, 1, OP_GREATER,
                            OP_NOT, OP_RETURN},
            .expected_instruction_count = 9,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_NUMBER, .as.number = 1.0},
                    {EXPECT_NUMBER, .as.number = 2.0},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile compare operation: >= operator",
            .src = "(>= 2 1)",
            .expected_instructions =
                (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT, 0, 1, OP_LESS,
                            OP_NOT, OP_RETURN},
            .expected_instruction_count = 9,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_NUMBER, .as.number = 2.0},
                    {EXPECT_NUMBER, .as.number = 1.0},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile let expression",
            .src = "(let x 42)",
            .expected_instructions =
                (uint8_t[]){OP_CONSTANT, 0, 1, OP_SET_GLOBAL, 0, 0, OP_RETURN},
            .expected_instruction_count = 7,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_OBJ_STRING, .as.obj_string = "x"},
                    {EXPECT_NUMBER, .as.number = 42.0},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile let expression with get global",
            .src = "(let b (+ a 1))",
            .expected_instructions =
                (uint8_t[]){OP_GET_GLOBAL, 0, 1, OP_CONSTANT, 0, 2, OP_ADD,
                            OP_SET_GLOBAL, 0, 0, OP_RETURN},
            .expected_instruction_count = 11,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_OBJ_STRING, .as.obj_string = "b"},
                    {EXPECT_OBJ_STRING, .as.obj_string = "a"},
                    {EXPECT_NUMBER, .as.number = 1.0},
                },
            .expected_constant_size = 3,
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

        mu_assert("%s: Constant count should match expected size.",
                  chunk->constants.count == (int)test->expected_constant_size);
        if (test->expected_constant_size > 0) {
            mu_assert("%s: Constants should match expected constants.",
                      assert_constants(chunk, test->expected_constants,
                                       test->expected_constant_size) == NULL);
        }

        destroyVM(vm);
    }

    return NULL;
}

void compiler_suite(void) {
    printf("--- Compiler Suite ---\n");
    mu_run_test(test_compile);
}
