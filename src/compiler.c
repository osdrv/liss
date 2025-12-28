#include "compiler.h"

#include <errno.h>
#include <stdlib.h>

#include "common.h"
#include "scanner.h"
#include "token.h"

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
} Compiler;

static void advance(Compiler* compiler) {
    Parser* parser = compiler->parser;
    parser->previous = parser->current;

    for (;;) {
        parser->current = scanToken(&parser->scanner);
        if (parser->current.type != TOKEN_ERROR) break;

        parser->hadError = true;
        DEBUG_LOG("[line %d] Error: %s\n", parser->current.line,
                  parser->current.start);
    }
}

static void consume(Compiler* compiler, TokenType type, const char* message) {
    Parser* parser = compiler->parser;
    if (parser->current.type == type) {
        advance(compiler);
        return;
    }

    parser->hadError = true;
    DEBUG_LOG("[line %d] Error: %s\n", parser->current.line, message);
}

static Chunk* currentChunk(Compiler* compiler) {
    return &compiler->function->chunk;
}

static void emitByte(Compiler* compiler, uint8_t byte) {
    writeChunk(currentChunk(compiler), byte);
}

static void emitBytes(Compiler* compiler, uint8_t byte1, uint8_t byte2) {
    emitByte(compiler, byte1);
    emitByte(compiler, byte2);
}

static void emitConstant(Compiler* compiler, Value value) {
    Chunk* chunk = currentChunk(compiler);
    int constant = addConstant(chunk, value);
    emitBytes(compiler, OP_CONSTANT, (uint8_t)constant);
}

static void emitReturn(Compiler* compiler) { emitByte(compiler, OP_RETURN); }

static void endCompiler(Compiler* compiler) { emitReturn(compiler); }

static void parseNumber(Compiler* compiler) {
    double value = strtod(compiler->parser->previous.start, NULL);
    emitConstant(compiler, NUMBER_VAL(value));
}

static void parseExpression(Compiler* compiler) {
    switch (compiler->parser->current.type) {
        case TOKEN_NUMBER:
            advance(compiler);
            parseNumber(compiler);
            break;
        default:
            compiler->parser->hadError = true;
            DEBUG_LOG("[line %d] Error: Expected expression.\n",
                      compiler->parser->current.line);
            break;
    }
}

ObjFunction* compile(VM* vm, const char* source) {
    Parser parser;
    Compiler compiler;

    initScanner(&parser.scanner, source);
    compiler.parser = &parser;
    compiler.function = newFunction(vm);

    advance(&compiler);

    parseExpression(&compiler);

    consume(&compiler, TOKEN_EOF, "Expect the end of expression.");

    endCompiler(&compiler);

    return compiler.parser->hadError ? NULL : compiler.function;
}
