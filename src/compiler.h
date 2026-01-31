#ifndef liss_compiler_h
#define liss_compiler_h

#include "scanner.h"
#include "vm.h"

typedef struct {
    Scanner scanner;
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef struct Compiler Compiler;

struct Compiler {
    Compiler* enclosing;
    Parser* parser;
    ObjFunction* function;
    VM* vm;
};

ObjFunction* compile(VM* vm, const char* source);

#endif
