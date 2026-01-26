#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "vm.h"
#include "repl.h"

static void runFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(file_size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    if (bytes_read < file_size) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    buffer[bytes_read] = '\0';
    fclose(file);

    // TODO: make the stack size configurable
    VM* vm = newVM(256);
    if (vm == NULL) {
        fprintf(stderr, "Could not create VM.\n");
        exit(74);
    }
    InterpretResult result = interpret(vm, buffer);
    free(buffer);
    destroyVM(vm);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char* argv[]) {
    if (argc == 1) {
        // No file provided, run REPL
        runRepl();
    } else if (argc == 2) {
        // Run file
        runFile(argv[1]);
    } else {
        fprintf(stderr, "Usage: liss [script]\n");
        exit(64);
    }

    return 0;
}
