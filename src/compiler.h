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

typedef struct {
    Parser* parser;
    ObjFunction* function;
    VM* vm;
} Compiler;

ObjFunction* compile(VM* vm, const char* source);

#endif
