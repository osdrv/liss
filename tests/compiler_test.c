#include "compiler.h"

#include "chunk.h"
#include "common.h"
#include "minunit.h"
#include "opcode.h"
#include "value.h"
#include "vm.h"

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

static char* assert_num_constants(Chunk* chunk, double expected[],
                                  size_t expected_count) {
    mu_assert("Chunk constant count does not match expected count.",
              chunk->constants.count == (int)expected_count);
    for (size_t i = 0; i < expected_count; i++) {
        Value val = chunk->constants.values[i];
        mu_assert("Chunk constant is not a number.", IS_NUMBER(val));
        mu_assert("Chunk constant value does not match expected value.",
                  AS_NUMBER(val) == expected[i]);
    }

    return NULL;
}

// A placeholder test for compiling a number.
// This test will fail until the compiler is implemented.
static char* test_compile_number(void) {
    VM* vm = newVM(256);  // The compiler needs a VM for GC management.

    const char* source = "123";
    ObjFunction* function = compile(vm, source);

    mu_assert("Compiler should not fail on a simple number.", function != NULL);

    Chunk* chunk = &function->chunk;
    mu_assert(
        "Chunk should have 3 bytes for: OP_CONSTANT, its operand and "
        "OP_RETURN.",
        chunk->count == 3);
    mu_assert("First byte should be OP_CONSTANT.",
              chunk->code[0] == OP_CONSTANT);
    mu_assert("Constant value should be 123.0.",
              AS_NUMBER(chunk->constants.values[chunk->code[1]]) == 123.0);

    destroyVM(vm);
    return NULL;
}

static char* test_compile_sum(void) {
    VM* vm = newVM(256);

    const char* source = "(- (+ 1 2 3) 4)";
    ObjFunction* function = compile(vm, source);

    mu_assert("Compiler should not fail on a simple sum.", function != NULL);

    Chunk* chunk = &function->chunk;
    mu_assert("Chunk should have 12 bytes", chunk->count == 12);
    mu_assert(
        "Should match instructions",
        assert_instructions(
            chunk,
            (uint8_t[]){OP_CONSTANT, 0, OP_CONSTANT, 1, OP_ADD, OP_CONSTANT, 2,
                        OP_ADD, OP_CONSTANT, 3, OP_SUBTRACT, OP_RETURN},
            12) == NULL);
    mu_assert("Should match constants",
              assert_num_constants(chunk, (double[]){1, 2, 3, 4}, 4) == NULL);

    return NULL;
}

void compiler_suite(void) {
    printf("--- Compiler Suite ---\n");
    // For now, this test is commented out because the compiler is not yet
    // implemented. Once you start implementing the compiler, you can uncomment
    // this to see it fail and then pass.
    mu_run_test(test_compile_number);
    mu_run_test(test_compile_sum);
}
