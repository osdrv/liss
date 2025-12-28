#include "compiler.h"

#include "chunk.h"
#include "minunit.h"
#include "opcode.h"
#include "value.h"
#include "vm.h"

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

void compiler_suite(void) {
    printf("--- Compiler Suite ---\n");
    // For now, this test is commented out because the compiler is not yet
    // implemented. Once you start implementing the compiler, you can uncomment
    // this to see it fail and then pass.
    mu_run_test(test_compile_number);
}
