#include "compiler.h"

#include <errno.h>
#include <stdlib.h>

#include "common.h"
#include "opcode.h"
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

static void parseExpression(Compiler* compiler);

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
    (void)message;  // Suppress unused parameter warning
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

static int emitJump(Compiler* compiler, OpCode op) {
    emitByte(compiler, op);
    emitBytes(compiler, 0xFF, 0xFF);
    return currentChunk(compiler)->count - 2;
}

static void patchJump(Compiler* compiler, int offset) {
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk(compiler)->count - offset - 2;

    if (jump > UINT16_MAX) {
        compiler->parser->hadError = true;
        DEBUG_LOG("[line %d] Error: Too much code to jump over.\n",
                  compiler->parser->current.line);
    }

    currentChunk(compiler)->code[offset] = (jump >> 8) & 0xff;
    currentChunk(compiler)->code[offset + 1] = jump & 0xff;
}

static void endCompiler(Compiler* compiler) { emitReturn(compiler); }

static void parseNumber(Compiler* compiler) {
    double value = strtod(compiler->parser->previous.start, NULL);
    emitConstant(compiler, NUMBER_VAL(value));
}

static void parseAnd(Compiler* compiler) {
    int jump_list[100];
    int jump_count = 0;

    parseExpression(compiler);
    if (compiler->parser->hadError) return;

    while (compiler->parser->current.type != TOKEN_RPAREN) {
        int jump = emitJump(compiler, OP_JUMP_IF_FALSE);
        jump_list[jump_count++] = jump;
        emitByte(compiler, OP_POP);
        parseExpression(compiler);
        if (compiler->parser->hadError) return;
    }

    for (int i = 0; i < jump_count; i++) {
        patchJump(compiler, jump_list[i]);
    }
    consume(compiler, TOKEN_RPAREN, "Expect ')' after 'and' expression");

    // Keeping this one for future optimization reference
    // parseExpression(compiler);
    // if (compiler->parser->hadError) return;
    // // Even though 0 is a valid jump offset, it would be taken by the first
    // // operand anyway.
    // int jump_chain = 0;
    // while (compiler->parser->current.type != TOKEN_RPAREN) {
    //     int jump = emitJump(compiler, OP_JUMP_IF_FALSE, jump_chain);
    //     jump_chain = jump;
    //     emitByte(compiler, OP_POP);
    //     parseExpression(compiler);
    //     if (compiler->parser->hadError) return;
    // }
    // while (jump_chain > 0) {
    //     int prev_jump = (int)(currentChunk(compiler)->code[jump_chain] << 8)
    //     |
    //                     currentChunk(compiler)->code[jump_chain + 1];
    //     patchJump(compiler, jump_chain);
    //     jump_chain = prev_jump;
    // }
    // consume(compiler, TOKEN_RPAREN, "Expect ')' after 'and' expression");
    // return;
}

static void parseGrouping(Compiler* compiler) {
    advance(compiler);
    TokenType op = compiler->parser->previous.type;

    // Operands with short-circuit behavior
    if (op == TOKEN_AND_KW) {
        parseAnd(compiler);
        return;
    }

    parseExpression(compiler);

    // Unary operators case
    switch (op) {
        case TOKEN_NOT_OP:
        case TOKEN_NOT_KW:
            emitByte(compiler, OP_NOT);
            consume(compiler, TOKEN_RPAREN, "Expect ')' after expression");
            return;
        default:
            break;
    }

    while (compiler->parser->current.type != TOKEN_RPAREN) {
        parseExpression(compiler);
        switch (op) {
            case TOKEN_PLUS_OP:
            case TOKEN_PLUS_KW:
                emitByte(compiler, OP_ADD);
                break;
            case TOKEN_MINUS_OP:
            case TOKEN_MINUS_KW:
                emitByte(compiler, OP_SUBTRACT);
                break;
            case TOKEN_STAR_OP:
            case TOKEN_STAR_KW:
                emitByte(compiler, OP_MULTIPLY);
                break;
            case TOKEN_SLASH_OP:
            case TOKEN_SLASH_KW:
                emitByte(compiler, OP_DIVIDE);
                break;
            case TOKEN_NOT_OP:
            case TOKEN_NOT_KW:
                emitByte(compiler, OP_NOT);
                break;
            default:
                compiler->parser->hadError = true;
                DEBUG_LOG("[line %d] Error: Unknown operator in grouping.\n",
                          compiler->parser->current.line);
                return;
        }
    }
    consume(compiler, TOKEN_RPAREN, "Expect ')' after expression");
}

static void parseExpression(Compiler* compiler) {
    switch (compiler->parser->current.type) {
        case TOKEN_NUMBER:
            advance(compiler);
            parseNumber(compiler);
            break;
        case TOKEN_TRUE_KW:
            advance(compiler);
            emitByte(compiler, OP_TRUE);
            break;
        case TOKEN_FALSE_KW:
            advance(compiler);
            emitByte(compiler, OP_FALSE);
            break;
        case TOKEN_NULL_KW:
            advance(compiler);
            emitByte(compiler, OP_NULL);
            break;
        case TOKEN_LPAREN:
            advance(compiler);
            parseGrouping(compiler);
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
