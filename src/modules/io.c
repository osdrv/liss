#include "io.h"

#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "vm.h"

/**
 * Prints the given values to a file or stdout.
 * If the first argument is a file handle, prints to it.
 *
 * Arguments: [File handle (optional), ...Values]
 * Return type: Nil
 */
static Value printNative(VM* vm, int argc, Value* args) {
    if (argc == 0) return NIL_VAL;

    FILE* out = stdout;
    int start_ix = 0;

    if (IS_FILE(args[0])) {
        ObjFile* file = AS_FILE(args[0]);
        if (file->is_closed) {
            return raiseErr(vm, "io:print: attempt to print to closed file");
        }
        out = file->file;
        start_ix = 1;
    }

    for (int i = start_ix; i < argc; i++) {
        char* str = sprintValue(args[i]);
        fprintf(out, "%s", str);
        free(str);
    }

    return NIL_VAL;
}

/**
 * Prints the given values followed by a newline.
 * If the first argument is a file handle, prints to it.
 *
 * Arguments: [File handle (optional), ...Values]
 * Return type: Nil
 */
static Value printlnNative(VM* vm, int argc, Value* args) {
    printNative(vm, argc, args);
    FILE* out = stdout;
    if (argc > 0 && IS_FILE(args[0])) {
        out = AS_FILE(args[0])->file;
    }
    fprintf(out, "\n");
    return NIL_VAL;
}

/**
 * Opens a file with the given path and mode.
 *
 * Arguments: [Path: String, Mode: String]
 * Return type: File Handle
 */
static Value openNative(VM* vm, int argc, Value* argv) {
    if (argc != 2 || !IS_STRING(argv[0]) || !IS_STRING(argv[1])) {
        return raiseErr(vm, "io:open: expect path and mode as strings");
    }
    FILE* file = fopen(AS_CSTRING(argv[0]), AS_CSTRING(argv[1]));
    if (file == NULL) {
        return raiseErr(vm, "io:open: could not open file");
    }
    return OBJ_VAL(newFile(vm, file));
}

/**
 * Closes the given file handle.
 *
 * Arguments: [Handle: File]
 * Return type: Nil
 */
static Value closeNative(VM* vm, int argc, Value* argv) {
    if (argc != 1 || !IS_FILE(argv[0])) {
        return raiseErr(vm, "io:close: expect file handle");
    }
    ObjFile* file = AS_FILE(argv[0]);
    if (file->is_closed) return NIL_VAL;

    fclose(file->file);
    file->is_closed = true;
    return NIL_VAL;
}

/**
 * Reads from a file handle. If byte_size is omitted, reads until EOF.
 *
 * Arguments: [Handle: File, byte_size: Int (optional)]
 * Return type: String
 */
static Value readNative(VM* vm, int argc, Value* argv) {
    if (argc < 1 || argc > 2 || !IS_FILE(argv[0])) {
        return raiseErr(vm, "io:read: expect file handle and optional byte_size");
    }
    ObjFile* file = AS_FILE(argv[0]);
    if (file->is_closed) {
        return raiseErr(vm, "io:read: read from closed file");
    }

    long size = 0;
    if (argc == 1) {
        long cur = ftell(file->file);
        if (fseek(file->file, 0, SEEK_END) != 0) {
            return raiseErr(vm, "io:read: seek failed");
        }
        long total = ftell(file->file);
        if (fseek(file->file, cur, SEEK_SET) != 0) {
            return raiseErr(vm, "io:read: seek failed");
        }
        size = total - cur;
        if (size <= 0) return OBJ_VAL(copyString(vm, "", 0));
    } else {
        if (!IS_INT(argv[1])) {
            return raiseErr(vm, "io:read: byte_size must be an integer");
        }
        size = AS_INT(argv[1]);
        if (size < 0) {
            return raiseErr(vm, "io:read: byte_size must be >= 0");
        }
    }

    char* buf = malloc(size + 1);
    if (buf == NULL) {
        return raiseErr(vm, "io:read: memory allocation failed");
    }

    size_t bytes_read = fread(buf, 1, size, file->file);
    buf[bytes_read] = '\0';

    return OBJ_VAL(takeString(vm, buf, (int)bytes_read));
}

/**
 * Reads a single line from the given file handle.
 *
 * Arguments: [Handle: File]
 * Return type: String
 */
static Value readLineNative(VM* vm, int argc, Value* argv) {
    if (argc != 1 || !IS_FILE(argv[0])) {
        return raiseErr(vm, "io:read-line: expect file handle");
    }
    ObjFile* file = AS_FILE(argv[0]);
    if (file->is_closed) {
        return raiseErr(vm, "io:read-line: read from closed file");
    }

    char* line = NULL;
    size_t cap = 0;
    size_t len = getline(&line, &cap, file->file);

    if (len == -1) {
        free(line);
        return OBJ_VAL(copyString(vm, "", 0));
    }

    // Strip newline if present
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
        len--;
    }
    if (len > 0 && line[len - 1] == '\r') {
        line[len - 1] = '\0';
        len--;
    }

    Value res = OBJ_VAL(copyString(vm, line, (int)len));
    free(line);
    return res;
}

/**
 * Seeks to a position in a file handle.
 *
 * Arguments: [Handle: File, Offset: Int, Whence: Int]
 * Return type: Nil
 */
static Value seekNative(VM* vm, int argc, Value* argv) {
    if (argc != 3 || !IS_FILE(argv[0]) || !IS_INT(argv[1]) || !IS_INT(argv[2])) {
        return raiseErr(vm, "io:seek: expect handle, offset (int), and whence (int)");
    }
    ObjFile* file = AS_FILE(argv[0]);
    if (file->is_closed) return raiseErr(vm, "io:seek: seek on closed file");

    if (fseek(file->file, AS_INT(argv[1]), (int)AS_INT(argv[2])) != 0) {
        return raiseErr(vm, "io:seek: operation failed");
    }
    return NIL_VAL;
}

/**
 * Returns the current position in a file handle.
 *
 * Arguments: [Handle: File]
 * Return type: Int
 */
static Value tellNative(VM* vm, int argc, Value* argv) {
    if (argc != 1 || !IS_FILE(argv[0])) {
        return raiseErr(vm, "io:tell: expect file handle");
    }
    ObjFile* file = AS_FILE(argv[0]);
    if (file->is_closed) return raiseErr(vm, "io:tell: tell on closed file");

    long pos = ftell(file->file);
    if (pos == -1) return raiseErr(vm, "io:tell: operation failed");
    return INT_VAL(pos);
}

static const NativeReg io_functions[] = {
    {"print", -1, printNative},
    {"println", -1, printlnNative},
    {"open", 2, openNative},
    {"close", 1, closeNative},
    {"read", -1, readNative},
    {"read-line", 1, readLineNative},
    {"seek", 3, seekNative},
    {"tell", 1, tellNative},
    {NULL, 0, NULL},  // Sentinel value
};

void registerIONatives(VM* vm, ObjModule* module) {
    defineNatives(vm, module, io_functions);

    // Standard pre-opened streams
    defineConst(vm, module, "stdin", OBJ_VAL(newFile(vm, stdin)));
    defineConst(vm, module, "stdout", OBJ_VAL(newFile(vm, stdout)));
    defineConst(vm, module, "stderr", OBJ_VAL(newFile(vm, stderr)));

    // IO modes
    defineConst(vm, module, "R", OBJ_VAL(copyString(vm, "r", 1)));
    defineConst(vm, module, "W", OBJ_VAL(copyString(vm, "w", 1)));
    defineConst(vm, module, "A", OBJ_VAL(copyString(vm, "a", 1)));
    defineConst(vm, module, "RW", OBJ_VAL(copyString(vm, "r+", 2)));

    // Seek constants
    defineConst(vm, module, "SET", INT_VAL(SEEK_SET));
    defineConst(vm, module, "CUR", INT_VAL(SEEK_CUR));
    defineConst(vm, module, "END", INT_VAL(SEEK_END));
}
