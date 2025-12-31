#include "compiler.h"

#include "chunk.h"
#include "common.h"
#include "minunit.h"
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
        mu_assert("Chunk instruction does not match expected instruction.",
                  chunk->code[i] == expected[i]);
    }

    return NULL;
}

static char* assert_values_equal(Value* a, Value* b) {
    mu_assert("Value types do not match.", a->type == b->type);
    switch (a->type) {
        case VAL_NUMBER:
            mu_assert("Number values do not match.",
                      AS_NUMBER(*a) == AS_NUMBER(*b));
            break;
        case VAL_BOOL:
            mu_assert("Boolean values do not match.",
                      AS_BOOL(*a) == AS_BOOL(*b));
            break;
        case VAL_NIL:
            // Nil values are always equal.
            break;
        case VAL_OBJ:
            mu_assert("Object values do not match.", AS_OBJ(*a) == AS_OBJ(*b));
            break;
    }
    return NULL;
}

static char* assert_num_constants(Chunk* chunk, Value expected[],
                                  size_t expected_count) {
    mu_assert("Chunk constant count does not match expected count.",
              chunk->constants.count == (int)expected_count);
    for (size_t i = 0; i < expected_count; i++) {
        Value val = chunk->constants.values[i];
        mu_assert("Values must be equal.",
                  assert_values_equal(&val, &expected[i]) == NULL);
    }

    return NULL;
}
static CompilerTestCase compile_tests[] = {
    {
        .name = "compile simple number",
        .src = "123",
        .expected_instructions = (uint8_t[]){OP_CONSTANT, 0, OP_RETURN},
        .expected_instruction_count = 3,
        .expected_constants = (Value[]){NUMBER_VAL(123.0)},
        .expected_constant_count = 1,
    },
    {
        .name = "compile simple addition",
        .src = "(+ 1 2)",
        .expected_instructions =
            (uint8_t[]){OP_CONSTANT, 0, OP_CONSTANT, 1, OP_ADD, OP_RETURN},
        .expected_instruction_count = 6,
        .expected_constants = (Value[]){NUMBER_VAL(1.0), NUMBER_VAL(2.0)},
        .expected_constant_count = 2,
    },
    {
        .name = "compile nested expression",
        .src = "(- (+ 10 5) 3)",
        .expected_instructions =
            (uint8_t[]){OP_CONSTANT, 0, OP_CONSTANT, 1, OP_ADD, OP_CONSTANT, 2,
                        OP_SUBTRACT, OP_RETURN},
        .expected_instruction_count = 9,
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
        .expected_instructions = (uint8_t[]){OP_TRUE, OP_JUMP_IF_FALSE, 0, 2,
                                             OP_POP, OP_FALSE, OP_RETURN},
        .expected_instruction_count = 7,
        .expected_constants = NULL,
        .expected_constant_count = 0,
    },
    {
        .name = "compile AND expression with more operands",
        .src = "(and true true false)",
        .expected_instructions =
            (uint8_t[]){OP_TRUE, OP_JUMP_IF_FALSE, 0, 7, OP_POP, OP_TRUE,
                        OP_JUMP_IF_FALSE, 0, 2, OP_POP, OP_FALSE, OP_RETURN},
        .expected_instruction_count = 12,
        .expected_constants = NULL,
        .expected_constant_count = 0,
    },
    {
        .name = "compile AND with all true",
        .src = "(and true true true)",
        .expected_instructions =
            (uint8_t[]){OP_TRUE, OP_JUMP_IF_FALSE, 0, 7, OP_POP, OP_TRUE,
                        OP_JUMP_IF_FALSE, 0, 2, OP_POP, OP_TRUE, OP_RETURN},
        .expected_instruction_count = 12,
        .expected_constants = NULL,
        .expected_constant_count = 0,
    },
    {
        .name = "compile AND with all false",
        .src = "(and false false false)",
        .expected_instructions =
            (uint8_t[]){OP_FALSE, OP_JUMP_IF_FALSE, 0, 7, OP_POP, OP_FALSE,
                        OP_JUMP_IF_FALSE, 0, 2, OP_POP, OP_FALSE, OP_RETURN},
        .expected_instruction_count = 12,
        .expected_constants = NULL,
        .expected_constant_count = 0,
    },
    {
        .name = "compile OR expression with 2 operands",
        .src = "(or false true)",
        .expected_instructions = (uint8_t[]){OP_FALSE, OP_JUMP_IF_FALSE, 0, 3,
                                             OP_JUMP, 0, 2, OP_POP, OP_TRUE, OP_RETURN},
        .expected_instruction_count = 10,
        .expected_constants = NULL,
        .expected_constant_count = 0,
    },
    {
        .name = "compile OR expression with more operands",
        .src = "(or false false true)",
        .expected_instructions =
            (uint8_t[]){OP_FALSE, OP_JUMP_IF_FALSE, 0, 3, OP_JUMP, 0, 10,
                        OP_POP, OP_FALSE, OP_JUMP_IF_FALSE, 0, 3, OP_JUMP, 0, 2,
                        OP_POP, OP_TRUE, OP_RETURN},
        .expected_instruction_count = 18,
        .expected_constants = NULL,
        .expected_constant_count = 0,
    },
};

static char* test_compile(void) {
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
        mu_assert("Instruction count should match expected count.",
                  chunk->count == (int)test->expected_instruction_count);
        mu_assert(
            "Instructions should match expected instructions.",
            assert_instructions(chunk, test->expected_instructions,
                                test->expected_instruction_count) == NULL);
        mu_assert("Constant count should match expected count.",
                  chunk->constants.count == (int)test->expected_constant_count);
        mu_assert("Constants should match expected constants.",
                  assert_num_constants(chunk, test->expected_constants,
                                       test->expected_constant_count) == NULL);

        destroyVM(vm);
    }

    return NULL;
}

void compiler_suite(void) {
    printf("--- Compiler Suite ---\n");
    mu_run_test(test_compile);
}
