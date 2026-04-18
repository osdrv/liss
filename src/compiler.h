#ifndef liss_compiler_h
#define liss_compiler_h

#include "scanner.h"
#include "vm.h"

#define MAX_LOCALS 256
#define MAX_GLOBALS 1024
#define MAX_UPVALUES 256
#define MAX_ARITY 255

typedef struct {
    Scanner scanner;
    Token current;
    Token previous;
    Token next;
    bool hadError;
    bool panicMode;
} Parser;

typedef struct {
    Token name;
    int depth;
} Local;

typedef struct {
    uint8_t index;
    bool is_local;
} Upvalue;

typedef struct Compiler Compiler;

struct Compiler {
    Compiler* enclosing;
    Parser* parser;
    ObjFunction* function;
    VM* vm;
    ObjModule* module;

    Table aliases;  // Maps module aliases to module objects

    Local locals[MAX_LOCALS];
    int local_count;
    int scope_depth;

    int upvalue_cnt;
    Upvalue upvalues[MAX_UPVALUES];

    int added_globals_cnt;
    Value added_globals[MAX_GLOBALS];
};

ObjFunction* compile(VM* vm, const char* source, ObjModule* module);
void markCompilerRoots(VM* vm);

#endif
