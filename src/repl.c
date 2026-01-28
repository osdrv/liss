#include "repl.h"

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "value.h"
#include "vm.h"

void runRepl() {
    // TODO: make the stack size configurable
    VM* vm = newVM(256);

    char line[1024];

    printf("LISS REPL. Type Ctrl+D (EOF) to exit.\n");

    for (;;) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        InterpretResult result = interpret(vm, line);
        if (result == INTERPRET_RUNTIME_ERROR) {
            // Print the last popped value if available
            if (vm->last_popped_value.type != VAL_NIL) {
                printf("Runtime error: ");
                printValue(vm->last_popped_value);
                printf("\n");
            }
        } else if (result == INTERPRET_OK) {
            // Print the last popped value
            printValue(vm->last_popped_value);
            printf("\n");
        }
    }

    destroyVM(vm);
}
