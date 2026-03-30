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

static bool isFlag(const char* arg) { return arg[0] == '-' && arg[1] == '-'; }

static VMOptions parseVMFlags(int argc, const char* argv[]) {
    VMOptions options = defaultVMOptions();
    for (int i = 1; i < argc; i++) {
        if (!isFlag(argv[i])) {
            continue;
        }
        if (strcmp(argv[i], "--stack-capacity") == 0) {
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

    char* file_name = NULL;
    for (int i = 1; i < argc; i++) {
        if (!isFlag(argv[i])) {
            file_name = argv[i];
            break;
        }
    }

    VMOptions options = parseVMFlags(argc, argv);

    if (file_name == NULL) {
        // No file provided, run REPL
        runRepl(options);
    } else if (argc > 1) {
        // Run file
        runFile(file_name, options);
    } else {
        fprintf(stderr, "Usage: liss [script]\n");
        exit(64);
    }

    return 0;
}
