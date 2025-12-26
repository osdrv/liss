#include <stdio.h>

#include "common.h"
#include "vm.h"

int main(int argc, const char* argv[]) {
    (void)argc;
    (void)argv;

    VM* vm = newVM(STACK_MAX);
    if (vm == NULL) {
        fprintf(stderr, "Could not create VM.\n");
        return 1;
    }

    DEBUG_LOG("--- Running VM ---");
    interpret(vm, "1 + 2");
    DEBUG_LOG("--- VM Finished ---");

    destroyVM(vm);

    return 0;
}
