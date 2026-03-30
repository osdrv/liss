#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "repl.h"
#include "vm.h"

void intHandler(int dummy) {
    (void)dummy;  // Suppress unused parameter warning
    printf("Exiting now...\n");
    exit(0);
}

static VMOptions parseVMFlags(int argc, const char* argv[]) {
    VMOptions options = defaultVMOptions();
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--stack-size") == 0) {
            options.stack_capacity = (size_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--gc-threshold") == 0) {
            options.gc_threshold = (size_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--heap-growth-factor") == 0) {
            options.heap_growth_factor = atof(argv[++i]);
        } else if (strcmp(argv[i], "--stress-gc") == 0) {
            options.stress_gc = true;
        } else {
            fprintf(stderr, "Unknown flag: %s\n", argv[i]);
            exit(64);
        }
    }
    return options;
}

static void runFile(const char* path, VMOptions options) {
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

    VM* vm = newVM(options);
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
    signal(SIGINT, intHandler);

    VMOptions options = parseVMFlags(argc, argv);

    if (argc == 1) {
        // No file provided, run REPL
        runRepl(options);
    } else if (argc == 2) {
        // Run file
        runFile(argv[1], options);
    } else {
        fprintf(stderr, "Usage: liss [script]\n");
        exit(64);
    }

    return 0;
}
