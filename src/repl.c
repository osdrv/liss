#include "repl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "value.h"
#include "vm.h"

void runRepl(VMOptions options) {
    VM* vm = newVM(options);
    char line[1024];

    PRINTF("LISS REPL. Type Ctrl+D (EOF) to exit.\n");

    for (;;) {
        PRINTF("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            PRINTF("\n");
            break;
        }
        InterpretResult result = interpret(vm, line, NULL);
        if (result == INTERPRET_COMPILE_ERROR) {
            ERROR_LOG("%s", vm->error_msg);
        } else if (result == INTERPRET_RUNTIME_ERROR) {
            char* str = sprintValue(vm->raise_value);
            ERROR_LOG("%s", str);
            free(str);
        } else if (result == INTERPRET_OK) {
            // Print the last popped value
            char* str = sprintValue(vm->last_popped_value);
            PRINTF("%s", str);
            free(str);
        }
    }

    destroyVM(vm);
}
