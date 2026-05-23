#include "compiler.h"

#include <stdint.h>
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
    EXPECT_INT,
    EXPECT_REAL,
    EXPECT_OBJ_STRING,
    EXPECT_OBJ_FUNCTION,
} ExpectedConstantType;

typedef struct {
    ExpectedConstantType type;
    union {
        int64_t integer;
        double real;
        const char* obj_string;
        const char* obj_function;
    } as;
} ExpectedConstant;

typedef struct {
    const char* name;
    const char* src;
    uint8_t* expected_instructions;
    size_t expected_instruction_count;
    ExpectedConstant* expected_constants;
    size_t expected_constant_size;
    bool is_failing;
} CompilerTestCase;

static char* assert_instructions(Chunk* chunk, uint8_t expected[],
                                 size_t expected_count) {
    mu_assert("Chunk instruction count does not match expected count.",
              chunk->count == (int)expected_count);

    for (size_t i = 0; i < expected_count; i++) {
        if (chunk->code[i] != expected[i]) {
            DEBUG_LOG("At instruction %zu: expected %d but got %d", i,
                      expected[i], chunk->code[i]);

            char* str = sprintChunk(chunk);
            DEBUG_LOG("%s", str);
            free(str);
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

static char* assert_int(Value value, int64_t expected) {
    mu_assert("Value is not an integer.", IS_INT(value));
    mu_assert("Integer value does not match expected.",
              AS_INT(value) == expected);
    return NULL;
}

static char* assert_real(Value value, double expected) {
    mu_assert("Value is not a real number.", IS_REAL(value));
    mu_assert("Real value does not match expected.",
              AS_REAL(value) == expected);
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

static char* assert_function(Value* value, const char* expected) {
    mu_assert("Value is not an object.", IS_OBJ(*value));
    mu_assert("Object is not a function.", OBJ_TYPE(*value) == OBJ_FUNCTION);
    ObjFunction* function = AS_FUNCTION(*value);
    if (expected != NULL) {
        mu_assert("Function name does not match expected.",
                  function->name != NULL &&
                      strcmp(function->name->chars, expected) == 0);
    } else {
        mu_assert("Function name is not NULL.", function->name == NULL);
    }
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
            case EXPECT_INT:
                mu_assert("Constant verification failed for number.",
                          assert_int(actual, exp.as.integer) == NULL);
                break;
            case EXPECT_REAL:
                mu_assert("Constant verification failed for real.",
                          assert_real(actual, exp.as.real) == NULL);
                break;
            case EXPECT_OBJ_STRING:
                mu_assert("Constant verification failed for string.",
                          assert_string(actual, exp.as.obj_string) == NULL);
                break;
            case EXPECT_OBJ_FUNCTION:
                mu_assert(
                    "Constant verification failed for function.",
                    assert_function(&actual, exp.as.obj_function) == NULL);
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
                    {EXPECT_INT, .as.integer = 123},
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
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
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
                    {EXPECT_INT, .as.integer = 10},
                    {EXPECT_INT, .as.integer = 5},
                    {EXPECT_INT, .as.integer = 3},
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
            .name = "compile binary kw and expression",
            .src = "(band 1 2)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_BAND,
                    OP_RETURN,
                },
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile binary op and expression",
            .src = "(&& 1 2)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_BAND,
                    OP_RETURN,
                },
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile binary kw or expression",
            .src = "(bor 1 2)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_BOR,
                    OP_RETURN,
                },
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile binary op or expression",
            .src = "(|| 1 2)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_BOR,
                    OP_RETURN,
                },
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile binary kw xor expression",
            .src = "(bxor 1 2)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_BXOR,
                    OP_RETURN,
                },
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile binary op xor expression",
            .src = "(^ 1 2)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_BXOR,
                    OP_RETURN,
                },
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile binary kw not expression",
            .src = "(bnot 1)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_BNOT,
                    OP_RETURN,
                },
            .expected_instruction_count = 5,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 1},
                },
            .expected_constant_size = 1,
        },
        {
            .name = "compile binary op xor expression",
            .src = "(~ 1)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_BNOT,
                    OP_RETURN,
                },
            .expected_instruction_count = 5,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 1},
                },
            .expected_constant_size = 1,
        },
        {
            .name = "compile binary kw lshift expression",
            .src = "(bsl 1 2)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_LSHIFT,
                    OP_RETURN,
                },
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile binary op lshift expression",
            .src = "(<< 1 2)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_LSHIFT,
                    OP_RETURN,
                },
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile binary kw rshift expression",
            .src = "(bsr 1 2)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_RSHIFT,
                    OP_RETURN,
                },
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile binary op rshift expression",
            .src = "(>> 1 2)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_RSHIFT,
                    OP_RETURN,
                },
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
                },
            .expected_constant_size = 2,
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
                    {EXPECT_INT, .as.integer = 123},
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
                    {EXPECT_INT, .as.integer = 123},
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
                    {EXPECT_INT, .as.integer = 123},
                    {EXPECT_INT, .as.integer = 456},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile compare operation: = operator",
            .src = "(= 1 2)",
            .expected_instructions = (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT,
                                                 0, 1, OP_EQUAL, OP_RETURN},
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
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
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
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
                    {EXPECT_INT, .as.integer = 2},
                    {EXPECT_INT, .as.integer = 1},
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
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
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
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
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
                    {EXPECT_INT, .as.integer = 2},
                    {EXPECT_INT, .as.integer = 1},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile let expression",
            .src = "(let x 42)",
            .expected_instructions =
                (uint8_t[]){OP_CONSTANT, 0, 0, OP_SET_GLOBAL, 0, 1, OP_RETURN},
            .expected_instruction_count = 7,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 42},
                    {EXPECT_OBJ_STRING, .as.obj_string = "x"},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile let expression with get global",
            .src = "(let b (+ a 1))",
            .expected_instructions =
                (uint8_t[]){OP_GET_GLOBAL, 0, 0, OP_CONSTANT, 0, 1, OP_ADD,
                            OP_SET_GLOBAL, 0, 2, OP_RETURN},
            .expected_instruction_count = 11,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_OBJ_STRING, .as.obj_string = "a"},
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_OBJ_STRING, .as.obj_string = "b"},
                },
            .expected_constant_size = 3,
        },
        {
            .name = "unary minus",
            .src = "(* -1 -2)",
            .expected_instructions = (uint8_t[]){OP_CONSTANT, 0, 0, OP_CONSTANT,
                                                 0, 1, OP_MULTIPLY, OP_RETURN},
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = -1},
                    {EXPECT_INT, .as.integer = -2},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "empty function",
            .src = "(fn [])",
            .expected_instructions = (uint8_t[]){OP_CLOSURE, 0, 0, OP_RETURN},
            .expected_instruction_count = 4,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_OBJ_FUNCTION, .as.obj_function = NULL},
                },
            .expected_constant_size = 1,
        },
        {
            .name = "named function no params",
            .src = "(fn myFunc [])",
            .expected_instructions =
                (uint8_t[]){
                    OP_CLOSURE,
                    0,
                    0,
                    OP_SET_GLOBAL,
                    0,
                    1,
                    OP_RETURN,
                },
            .expected_instruction_count = 7,
            .expected_constant_size = 2,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_OBJ_FUNCTION, .as.obj_function = "myFunc"},
                    {EXPECT_OBJ_STRING, .as.obj_string = "myFunc"},
                },
        },
        {
            .name = "function with body",
            .src = "(fn addOne [x] (+ x 1))",
            .expected_instructions =
                (uint8_t[]){
                    OP_CLOSURE,
                    0,
                    0,
                    OP_SET_GLOBAL,
                    0,
                    1,
                    OP_RETURN,
                },
            .expected_instruction_count = 7,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_OBJ_FUNCTION, .as.obj_function = "addOne"},
                    {EXPECT_OBJ_STRING, .as.obj_string = "addOne"},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "function call no args",
            .src = "(fn greet [] \"Hello\") (greet)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CLOSURE,
                    0,
                    0,
                    OP_SET_GLOBAL,
                    0,
                    1,
                    OP_POP,
                    OP_GET_GLOBAL,
                    0,
                    1,
                    OP_TAIL_CALL,
                    0,
                    OP_RETURN,
                },
            .expected_instruction_count = 13,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_OBJ_FUNCTION, .as.obj_function = "greet"},
                    {EXPECT_OBJ_STRING, .as.obj_string = "greet"},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "anonymous function call",
            .src = "((fn [x] (+ x 1)) 41)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CLOSURE,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_TAIL_CALL,
                    1,
                    OP_RETURN,
                },
            .expected_instruction_count = 9,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_OBJ_FUNCTION, .as.obj_function = NULL},
                    {EXPECT_INT, .as.integer = 41},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "named function call",
            .src = "(fn addOne [x] (+ x 1)) (addOne 41)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CLOSURE,
                    0,
                    0,
                    OP_SET_GLOBAL,
                    0,
                    1,
                    OP_POP,
                    OP_GET_GLOBAL,
                    0,
                    1,
                    OP_CONSTANT,
                    0,
                    2,
                    OP_TAIL_CALL,
                    1,
                    OP_RETURN,
                },
            .expected_instruction_count = 16,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_OBJ_FUNCTION, .as.obj_function = "addOne"},
                    {EXPECT_OBJ_STRING, .as.obj_string = "addOne"},
                    {EXPECT_INT, .as.integer = 41},
                },
            .expected_constant_size = 3,
        },
        {
            .name = "parse pair with string KV",
            .src = "(\"foo\" . \"bar\")",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_PAIR,
                    OP_RETURN,
                },
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_OBJ_STRING, .as.obj_string = "foo"},
                    {EXPECT_OBJ_STRING, .as.obj_string = "bar"},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "parse pair with int KV",
            .src = "(123 . 456)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_PAIR,
                    OP_RETURN,
                },
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 123},
                    {EXPECT_INT, .as.integer = 456},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "parse pair with negative int KV",
            .src = "(-123 . -456)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_PAIR,
                    OP_RETURN,
                },
            .expected_instruction_count = 8,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = -123},
                    {EXPECT_INT, .as.integer = -456},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "parse block of expressions",
            .src = "(1 2 3 4 5)",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT, 0, 0, OP_POP,    OP_CONSTANT, 0, 1, OP_POP,
                    OP_CONSTANT, 0, 2, OP_POP,    OP_CONSTANT, 0, 3, OP_POP,
                    OP_CONSTANT, 0, 4, OP_RETURN,
                },
            .expected_instruction_count = 20,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
                    {EXPECT_INT, .as.integer = 3},
                    {EXPECT_INT, .as.integer = 4},
                    {EXPECT_INT, .as.integer = 5},
                },
            .expected_constant_size = 5,
        },
        {
            .name = "compile try expression",
            .src = "(try (/ 42 0))",
            .expected_instructions =
                (uint8_t[]){
                    OP_TRY_START,
                    0,
                    8,
                    OP_CONSTANT,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_DIVIDE,
                    OP_TRY_END,
                    OP_RETURN,
                },
            .expected_instruction_count = 12,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 42},
                    {EXPECT_INT, .as.integer = 0},
                },
            .expected_constant_size = 2,
        },
        {
            .name = "compile an empty list",
            .src = "[]",
            .expected_instructions =
                (uint8_t[]){
                    OP_LIST,
                    0,
                    OP_RETURN,
                },
            .expected_instruction_count = 3,
            .expected_constants = NULL,
            .expected_constant_size = 0,
        },
        {
            .name = "compile a list with elements",
            .src = "[1 2 3]",
            .expected_instructions =
                (uint8_t[]){
                    OP_CONSTANT,
                    0,
                    0,
                    OP_CONSTANT,
                    0,
                    1,
                    OP_CONSTANT,
                    0,
                    2,
                    OP_LIST,
                    3,
                    OP_RETURN,
                },
            .expected_instruction_count = 12,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_INT, .as.integer = 1},
                    {EXPECT_INT, .as.integer = 2},
                    {EXPECT_INT, .as.integer = 3},
                },
            .expected_constant_size = 3,
        },
        {
            .name = "compile an empty dict",
            .src = "(dict)",
            .expected_instructions =
                (uint8_t[]){
                    OP_GET_GLOBAL,
                    0,
                    0,
                    OP_TAIL_CALL,
                    0,
                    OP_RETURN,
                },
            .expected_instruction_count = 6,
            .expected_constants =
                (ExpectedConstant[]){
                    {EXPECT_OBJ_STRING, .as.obj_string = "dict"},
                },
            .expected_constant_size = 1,
        },
    };

    for (size_t i = 0; i < sizeof(compile_tests) / sizeof(compile_tests[0]);
         i++) {
        CompilerTestCase* test = &compile_tests[i];
        DEBUG_LOG("Running test: %s", test->name);
        VMOptions options = {
            .stack_capacity = 64,
            .gc_threshold = 1024,
            .heap_growth_factor = 2,
            .stress_gc = true,
            .frames_max = 32,
        };
        VM* vm = newVM(options);

        if (test->is_failing) {
            DEBUG_LOG("Set a breakpoint for a failing test");
            __builtin_debugtrap();
        }

        ObjModule* test_module = newModule(vm, "test_module");
        ObjFunction* function = compile(vm, test->src, test_module);
        mu_assert("Compiler should not fail.", function != NULL);

        Chunk* chunk = &function->chunk;

        if (chunk->count != (int)test->expected_instruction_count) {
            ERROR_LOG("Unexpected instruction count");
            char* str = sprintChunk(chunk);
            DEBUG_LOG("%s", str);
            free(str);
        }
        mu_assert("Instruction count should match expected count.",
                  chunk->count == (int)test->expected_instruction_count);
        mu_assert(
            "Instructions should match expected instructions.",
            assert_instructions(chunk, test->expected_instructions,
                                test->expected_instruction_count) == NULL);

        mu_assert("Constant count should match expected size.",
                  chunk->constants.count == (int)test->expected_constant_size);
        if (test->expected_constant_size > 0) {
            const char* err = assert_constants(chunk, test->expected_constants,
                                               test->expected_constant_size);
            if (err != NULL) DEBUG_LOG("Constant assertion failed: %s", err);
            mu_assert("Constants should match expected constants.",
                      err == NULL);
        }

        destroyVM(vm);
    }

    return NULL;
}

void compiler_suite(void) {
    printf("--- Compiler Suite ---\n");
    mu_run_test(test_compile);
}
