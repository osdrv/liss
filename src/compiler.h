#ifndef liss_compiler_h
#define liss_compiler_h

#include "scanner.h"
#include "vm.h"

#define MAX_LOCALS 256

typedef struct {
    Scanner scanner;
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef struct {
    Token name;
    int depth;
} Local;

typedef struct Compiler Compiler;

struct Compiler {
    Compiler* enclosing;
    Parser* parser;
    ObjFunction* function;
    VM* vm;

    Local locals[MAX_LOCALS];
    int local_count;
    int scope_depth;
};

ObjFunction* compile(VM* vm, const char* source);

#endif
