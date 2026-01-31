#include "compiler.h"

#include <errno.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "object.h"
#include "opcode.h"
#include "token.h"

static void parseExpression(Compiler* compiler);

static void initParser(Parser* parser) {
    parser->hadError = false;
    parser->panicMode = false;
}

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

static Token consume(Compiler* compiler, TokenType type, const char* message) {
    Parser* parser = compiler->parser;
    Token token = parser->current;
    if (token.type == type) {
        advance(compiler);
        return token;
    }

    parser->hadError = true;
    DEBUG_LOG("[line %d] Error: %s\n", parser->current.line, message);
    (void)message;  // Suppress unused parameter warning

    return token;
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
    if (constant > UINT16_MAX) {
        compiler->parser->hadError = true;
        DEBUG_LOG("[line %d] Error: Too many constants in one chunk.\n",
                  compiler->parser->current.line);
        return;
    }
    emitByte(compiler, OP_CONSTANT);
    emitBytes(compiler, (uint8_t)(constant >> 8), (uint8_t)(constant & 0xff));
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

static void initCompiler(Compiler* compiler, Compiler* enclosing) {
    compiler->enclosing = enclosing;
    if (enclosing != NULL) {
        compiler->parser = enclosing->parser;
        compiler->vm = enclosing->vm;
    }
    compiler->function = newFunction(enclosing ? enclosing->vm : compiler->vm);
    push(enclosing ? enclosing->vm : compiler->vm, OBJ_VAL(compiler->function));
}

static ObjFunction* endCompiler(Compiler* compiler) {
    emitReturn(compiler);
    return compiler->function;
}

static void parseNumber(Compiler* compiler) {
    double value = strtod(compiler->parser->previous.start, NULL);
    emitConstant(compiler, NUMBER_VAL(value));
}

static void parseString(Compiler* compiler) {
    Token* token = &compiler->parser->previous;
    ObjString* string = copyString(compiler->vm, token->start, token->length);
    emitConstant(compiler, OBJ_VAL(string));
}

static void parseAnd(Compiler* compiler) {
    int jump_list[100];
    int jump_count = 0;

    parseExpression(compiler);
    if (compiler->parser->hadError) return;

    while (compiler->parser->current.type != TOKEN_RPAREN) {
        int jump = emitJump(compiler, OP_JUMP_IF_FALSE);
        jump_list[jump_count++] = jump;
        parseExpression(compiler);
        if (compiler->parser->hadError) return;
    }

    for (int i = 0; i < jump_count; i++) {
        patchJump(compiler, jump_list[i]);
    }
    consume(compiler, TOKEN_RPAREN, "Expect ')' after 'and' expression");
}

static void parseOr(Compiler* compiler) {
    if (compiler->parser->current.type == TOKEN_RPAREN) {
        compiler->parser->hadError = true;
        DEBUG_LOG(
            "[line %d] Error: 'or' expression requires at least one operand.\n",
            compiler->parser->current.line);
        return;
    }
    int jump_list[100];
    int jump_count = 0;

    parseExpression(compiler);
    if (compiler->parser->hadError) return;

    while (compiler->parser->current.type != TOKEN_RPAREN) {
        // If the previous expression is false, jump to the next operand
        int else_jump = emitJump(compiler, OP_JUMP_IF_FALSE);
        int end_jump = emitJump(compiler, OP_JUMP);
        jump_list[jump_count++] = end_jump;
        patchJump(compiler, else_jump);
        emitByte(compiler, OP_POP);
        parseExpression(compiler);
        if (compiler->parser->hadError) return;
    }

    for (int i = 0; i < jump_count; i++) {
        patchJump(compiler, jump_list[i]);
    }
    consume(compiler, TOKEN_RPAREN, "Expect ')' after 'or' expression");
}

static int parseVariable(Compiler* compiler) {
    Token identifier =
        consume(compiler, TOKEN_IDENTIFIER, "Expect identifier after 'let'.");
    if (compiler->parser->hadError) return -1;
    ObjString* var_name =
        copyString(compiler->vm, identifier.start, identifier.length);
    return addConstant(&compiler->function->chunk, OBJ_VAL(var_name));
}

static void parseLet(Compiler* compiler) {
    int var_index = parseVariable(compiler);
    if (compiler->parser->hadError) return;
    parseExpression(compiler);
    if (compiler->parser->hadError) return;
    emitByte(compiler, OP_SET_GLOBAL);
    emitBytes(compiler, (uint8_t)(var_index >> 8), (uint8_t)(var_index & 0xff));
    consume(compiler, TOKEN_RPAREN, "Expect ')' after 'let' expression");
    if (compiler->parser->hadError) return;
}

static void parseCond(Compiler* compiler) {
    // Parse condition
    parseExpression(compiler);
    if (compiler->parser->hadError) return;
    int else_jump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);

    // Parse then branch
    parseExpression(compiler);
    if (compiler->parser->hadError) return;
    int end_jump = emitJump(compiler, OP_JUMP);
    patchJump(compiler, else_jump);
    emitByte(compiler, OP_POP);

    // Parse else branch
    if (compiler->parser->current.type != TOKEN_RPAREN) {
        parseExpression(compiler);
        if (compiler->parser->hadError) return;
    } else {
        // If there's no else branch, we emit a null value
        emitByte(compiler, OP_NULL);
    }
    patchJump(compiler, end_jump);
    consume(compiler, TOKEN_RPAREN, "Expect ')' after 'cond' expression");
}

static void compileFunction(Compiler* compiler) {
    Compiler fn_compiler;
    initCompiler(&fn_compiler, compiler);

    if (fn_compiler.parser->current.type == TOKEN_IDENTIFIER) {
        Token fn_name = consume(&fn_compiler, TOKEN_IDENTIFIER,
                                "Expect function name after 'fn'.");
        fn_compiler.function->name =
            copyString(fn_compiler.vm, fn_name.start, fn_name.length);
    } else {
        fn_compiler.function->name = copyString(fn_compiler.vm, "fn", 2);
    }

    consume(&fn_compiler, TOKEN_LBRAKET, "Expect '[' for function parameters.");
    while (fn_compiler.parser->current.type != TOKEN_RBRAKET) {
        Token param_name =
            consume(&fn_compiler, TOKEN_IDENTIFIER, "Expect parameter name.");
        if (fn_compiler.parser->hadError) return;
        ObjString* param_str =
            copyString(fn_compiler.vm, param_name.start, param_name.length);
        // TODO: Do something with these parameters
    }
    consume(&fn_compiler, TOKEN_RBRAKET, "Expect ']' after parameters.");

    while (fn_compiler.parser->current.type != TOKEN_RPAREN) {
        parseExpression(&fn_compiler);
        if (fn_compiler.parser->hadError) return;
    }
    consume(&fn_compiler, TOKEN_RPAREN, "Expect ')' after function body.");

    ObjFunction* function = endCompiler(&fn_compiler);
    pop(compiler->vm);
    emitConstant(compiler, OBJ_VAL(function));
}

static void parseGrouping(Compiler* compiler) {
    advance(compiler);
    TokenType op = compiler->parser->previous.type;

    // Operands with short-circuit behavior
    switch (op) {
        case TOKEN_AND_KW:
            parseAnd(compiler);
            return;
        case TOKEN_OR_KW:
            parseOr(compiler);
            return;
        case TOKEN_COND_KW:
            parseCond(compiler);
            return;
        case TOKEN_LET_KW:
            parseLet(compiler);
            return;
        case TOKEN_FN_KW:
            compileFunction(compiler);
            return;
        default:
            break;
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
        if (compiler->parser->hadError) return;
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
            case TOKEN_EQUAL_OP:
            case TOKEN_EQUAL_KW:
                emitByte(compiler, OP_EQUAL);
                break;
            case TOKEN_NOT_EQUAL_OP:
            case TOKEN_NOT_EQUAL_KW:
                emitByte(compiler, OP_EQUAL);
                emitByte(compiler, OP_NOT);
                break;
            case TOKEN_GREATER_OP:
            case TOKEN_GREATER_KW:
                emitByte(compiler, OP_GREATER);
                break;
            case TOKEN_GREATER_EQUAL_OP:
            case TOKEN_GREATER_EQUAL_KW:
                emitByte(compiler, OP_LESS);
                emitByte(compiler, OP_NOT);
                break;
            case TOKEN_LESS_OP:
            case TOKEN_LESS_KW:
                emitByte(compiler, OP_LESS);
                break;
            case TOKEN_LESS_EQUAL_OP:
            case TOKEN_LESS_EQUAL_KW:
                emitByte(compiler, OP_GREATER);
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

static void namedVariable(Compiler* compiler, Token name) {
    ObjString* var_name = copyString(compiler->vm, name.start, name.length);
    int const_index =
        addConstant(&compiler->function->chunk, OBJ_VAL(var_name));

    if (const_index > UINT16_MAX) {
        compiler->parser->hadError = true;
        DEBUG_LOG("[line %d] Error: Too many constants in one chunk.\n",
                  compiler->parser->current.line);
        return;
    }

    emitByte(compiler, OP_GET_GLOBAL);
    emitBytes(compiler, (uint8_t)(const_index >> 8),
              (uint8_t)(const_index & 0xff));
}

static void parseExpression(Compiler* compiler) {
    switch (compiler->parser->current.type) {
        case TOKEN_NUMBER:
            advance(compiler);
            parseNumber(compiler);
            break;
        case TOKEN_STRING:
            advance(compiler);
            parseString(compiler);
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
        case TOKEN_IDENTIFIER:
            namedVariable(compiler, compiler->parser->current);
            advance(compiler);
            break;
        case TOKEN_MINUS_OP:
            // Unary minus
            advance(compiler);
            parseExpression(compiler);
            emitByte(compiler, OP_NEGATE);
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
    initParser(&parser);
    initScanner(&parser.scanner, source);

    Compiler compiler;
    compiler.vm = vm;
    compiler.parser = &parser;
    initCompiler(&compiler, NULL);

    advance(&compiler);

    do {
        parseExpression(&compiler);
        if (compiler.parser->hadError) break;
    } while (compiler.parser->current.type != TOKEN_EOF);

    if (!compiler.parser->hadError) {
        consume(&compiler, TOKEN_EOF, "Expect the end of expression.");
    }

    ObjFunction* function = endCompiler(&compiler);

    pop(vm);

    return parser.hadError ? NULL : function;
}
