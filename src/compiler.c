#include "compiler.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "object.h"
#include "opcode.h"
#include "token.h"
#include "vm.h"

static void parseExpression(Compiler* compiler);

static void initParser(Parser* parser) {
    parser->hadError = false;
    parser->panicMode = false;
    parser->previous = (Token){0};
    parser->current = (Token){0};
    parser->next = (Token){0};
}

// Function advance moves the parser forward.
// In case this is the first read, it will read two tokens to fill both current and next.
// If the current token is an error, it will report it and keep advancing until it finds a non-error token or reaches the end of the input.
static void advance(Compiler* compiler) {
    Parser* parser = compiler->parser;

    parser->previous = parser->current;
    parser->current = parser->next;

    for (;;) {
        parser->current = parser->next;
        parser->next = scanToken(&parser->scanner);
        // If the current token is TOKEN_ZERO, read one more token as it wasn't read yet.
        if (parser->current.type == TOKEN_ZERO) {
            continue;
        }
        if (parser->current.type != TOKEN_ERROR) break;

        parser->hadError = true;
        ERROR_LOG("[line %d] Error: %s", parser->current.line,
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
    ERROR_LOG("[line %d] Error: %s", parser->current.line, message);
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
        ERROR_LOG("[line %d] Error: Too many constants in one chunk",
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
        ERROR_LOG("[line %d] Error: Too much code to jump over",
                  compiler->parser->current.line);
    }

    currentChunk(compiler)->code[offset] = (jump >> 8) & 0xff;
    currentChunk(compiler)->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler* compiler, Compiler* enclosing) {
    compiler->enclosing = enclosing;
    compiler->local_count = 0;
    compiler->scope_depth = 0;

    if (enclosing != NULL) {
        compiler->parser = enclosing->parser;
        compiler->vm = enclosing->vm;
    }
    compiler->function = newFunction(compiler->vm);

    Local* local = &compiler->locals[compiler->local_count++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

static ObjFunction* endCompiler(Compiler* compiler) {
    emitReturn(compiler);
    return compiler->function;
}

static void beginScope(Compiler* compiler) { compiler->scope_depth++; }

static void endScope(Compiler* compiler) {
    compiler->scope_depth--;

    int locals_in_scope = 0;
    while (compiler->local_count > 0 &&
           compiler->locals[compiler->local_count - 1].depth >
               compiler->scope_depth) {
        locals_in_scope++;
        compiler->local_count--;
    }

    if (locals_in_scope > 0) {
        emitBytes(compiler, OP_SET_LOCAL, (uint8_t)compiler->local_count);
        // Pop all locals in scope except the last one
        for (int i = 0; i < locals_in_scope - 1; i++) {
            emitByte(compiler, OP_POP);
        }
    }
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
}

static void parseOr(Compiler* compiler) {
    if (compiler->parser->current.type == TOKEN_RPAREN) {
        compiler->parser->hadError = true;
        ERROR_LOG(
            "[line %d] Error: 'or' expression requires at least one operand",
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
}

static void addLocal(Compiler* compiler, Token name) {
    if (compiler->local_count >= MAX_LOCALS) {
        compiler->parser->hadError = true;
        ERROR_LOG("[line %d] Error: Too many local variables in function.",
                  compiler->parser->current.line);
        return;
    }
    Local* local = &compiler->locals[compiler->local_count++];
    local->name = name;
    local->depth = compiler->scope_depth;
}

static int resolveLocal(Compiler* compiler, Token name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (local->name.length == name.length &&
            memcmp(local->name.start, name.start, name.length) == 0) {
            return i;
        }
    }
    return -1;
}

static int identifierConstant(Compiler* compiler, Token name) {
    ObjString* var_name = copyString(compiler->vm, name.start, name.length);
    return addConstant(&compiler->function->chunk, OBJ_VAL(var_name));
}

static void parseLet(Compiler* compiler) {
    Token identifier =
        consume(compiler, TOKEN_IDENTIFIER, "expect an identifier after `let`");
    if (compiler->parser->hadError) return;

    parseExpression(compiler);
    if (compiler->parser->hadError) return;

    if (compiler->scope_depth > 0) {
        for (int i = compiler->local_count - 1; i >= 0; i--) {
            Local* local = &compiler->locals[i];
            if (local->depth != -1 && local->depth < compiler->scope_depth) {
                break;
            }
            if (local->name.length == identifier.length &&
                memcmp(local->name.start, identifier.start,
                       identifier.length) == 0) {
                compiler->parser->hadError = true;
                ERROR_LOG(
                    "[line %d] Error: Cannot redeclare variable '%.*s' in this "
                    "scope",
                    identifier.line, identifier.length, identifier.start);
                return;
            }
        }
        addLocal(compiler, identifier);
    } else {
        int var_index = identifierConstant(compiler, identifier);
        emitByte(compiler, OP_SET_GLOBAL);
        emitBytes(compiler, (uint8_t)(var_index >> 8),
                  (uint8_t)(var_index & 0xff));
    }
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
}

static ObjFunction* compileFunction(Compiler* compiler) {
    Compiler fn_compiler;
    initCompiler(&fn_compiler, compiler);

    push(compiler->vm, OBJ_VAL(fn_compiler.function));

    fn_compiler.scope_depth = compiler->scope_depth + 1;

    if (fn_compiler.parser->current.type == TOKEN_IDENTIFIER) {
        Token fn_name = consume(&fn_compiler, TOKEN_IDENTIFIER,
                                "expect function name after 'fn'");
        fn_compiler.function->name =
            copyString(fn_compiler.vm, fn_name.start, fn_name.length);
    } else {
        fn_compiler.function->name = NULL;
    }

    consume(&fn_compiler, TOKEN_LBRAKET, "expect '[' for function parameters");

    if (fn_compiler.parser->current.type != TOKEN_RBRAKET) {
        do {
            fn_compiler.function->arity++;
            if (fn_compiler.function->arity >= MAX_LOCALS) {
                compiler->parser->hadError = true;
                ERROR_LOG(
                    "[line %d] Error: max function parameter limit reached",
                    fn_compiler.parser->current.line);
                return NULL;
            }
            Token param = consume(&fn_compiler, TOKEN_IDENTIFIER,
                                  "Expect parameter name");
            addLocal(&fn_compiler, param);
        } while (fn_compiler.parser->current.type == TOKEN_IDENTIFIER);
    }

    consume(&fn_compiler, TOKEN_RBRAKET, "Expect ']' after parameters");

#define WILL_READ_BODY() (fn_compiler.parser->current.type != TOKEN_RPAREN)

    bool is_empty_body = true;
    while (WILL_READ_BODY()) {
        parseExpression(&fn_compiler);
        if (fn_compiler.parser->hadError) return NULL;
        is_empty_body = false;
        // We compare init_chunk_count to the current chunk count to determine
        // if the expression we just parsed emitted any bytecode. If it didn't,
        // we don't need to emit a POP instruction.
        if (WILL_READ_BODY()) {
            emitByte(&fn_compiler, OP_POP);
        }
    }
    if (is_empty_body) {
        // If the function body is empty, we emit a null value so it returns nil
        emitByte(&fn_compiler, OP_NULL);
    }

#undef WILL_READ_BODY

    ObjFunction* function = endCompiler(&fn_compiler);
    return function;
}

static void parseBlock(Compiler* compiler) {
    beginScope(compiler);
    while (compiler->parser->current.type != TOKEN_RPAREN) {
        parseExpression(compiler);
        if (compiler->parser->hadError) return;
        if (compiler->parser->current.type != TOKEN_RPAREN) {
            emitByte(compiler, OP_POP);
        }
    }
    endScope(compiler);
}

static void parseGrouping(Compiler* compiler) {
    switch (compiler->parser->current.type) {
        case TOKEN_AND_KW:
            advance(compiler);
            parseAnd(compiler);
            break;
        case TOKEN_OR_KW:
            advance(compiler);
            parseOr(compiler);
            break;
        case TOKEN_COND_KW:
            advance(compiler);
            parseCond(compiler);
            break;
        case TOKEN_LET_KW:
            advance(compiler);
            parseLet(compiler);
            break;
        case TOKEN_FN_KW:
            advance(compiler);
            Token fn_name = {0};
            bool is_named_fn = false;
            if (compiler->parser->current.type == TOKEN_IDENTIFIER) {
                fn_name = consume(compiler, TOKEN_IDENTIFIER,
                                  "expect function name after 'fn'");
                if (compiler->parser->hadError) return;
                is_named_fn = true;
                if (compiler->scope_depth > 0) {
                    addLocal(compiler, fn_name);
                }
            }

            ObjFunction* func = compileFunction(compiler);
            if (compiler->parser->hadError) return;
            if (is_named_fn) {
                func->name =
                    copyString(compiler->vm, fn_name.start, fn_name.length);
            }

            emitConstant(compiler, OBJ_VAL(func));

            pop(compiler->vm);

            if (is_named_fn) {
                if (compiler->scope_depth > 0) {
                    int local_slot = resolveLocal(compiler, fn_name);
                    if (local_slot == -1) {
                        compiler->parser->hadError = true;
                        ERROR_LOG(
                            "[line %d] Error: Failed to resolve local variable "
                            "for "
                            "function name '%.*s'",
                            fn_name.line, fn_name.length, fn_name.start);
                        return;
                    }
                    emitBytes(compiler, OP_SET_LOCAL, (uint8_t)local_slot);
                } else {
                    int var_name_ix = identifierConstant(compiler, fn_name);
                    emitByte(compiler, OP_SET_GLOBAL);
                    emitBytes(compiler, (uint8_t)(var_name_ix >> 8),
                              (uint8_t)(var_name_ix & 0xff));
                }
            }
            break;
        case TOKEN_NOT_OP:
        case TOKEN_NOT_KW:
            advance(compiler);
            parseExpression(compiler);
            if (compiler->parser->hadError) return;
            emitByte(compiler, OP_NOT);
            break;
        case TOKEN_PLUS_OP:
        case TOKEN_PLUS_KW:
        case TOKEN_MINUS_OP:
        case TOKEN_MINUS_KW:
        case TOKEN_STAR_OP:
        case TOKEN_STAR_KW:
        case TOKEN_SLASH_OP:
        case TOKEN_SLASH_KW:
        case TOKEN_EQUAL_OP:
        case TOKEN_EQUAL_KW:
        case TOKEN_NOT_EQUAL_OP:
        case TOKEN_NOT_EQUAL_KW:
        case TOKEN_GREATER_OP:
        case TOKEN_GREATER_KW:
        case TOKEN_GREATER_EQUAL_OP:
        case TOKEN_GREATER_EQUAL_KW:
        case TOKEN_LESS_OP:
        case TOKEN_LESS_KW:
        case TOKEN_LESS_EQUAL_OP:
        case TOKEN_LESS_EQUAL_KW: {
            TokenType op = compiler->parser->current.type;
            advance(compiler);
            parseExpression(compiler);
            if (compiler->parser->hadError) return;

            while (compiler->parser->current.type != TOKEN_RPAREN) {
                parseExpression(compiler);
                switch (op) {
                    // Tokens with 2+ arity:
                    case TOKEN_PLUS_OP:
                    case TOKEN_PLUS_KW:
                        emitByte(compiler, OP_ADD);
                        break;  // continue parsing more operands
                    case TOKEN_STAR_OP:
                    case TOKEN_STAR_KW:
                        emitByte(compiler, OP_MULTIPLY);
                        break;  // continue parsing more operands
                    // Tokens with 2 arity:
                    case TOKEN_MINUS_OP:
                    case TOKEN_MINUS_KW:
                        emitByte(compiler, OP_SUBTRACT);
                        goto END_PARSE_GROUPING;
                    case TOKEN_SLASH_OP:
                    case TOKEN_SLASH_KW:
                        emitByte(compiler, OP_DIVIDE);
                        goto END_PARSE_GROUPING;
                    case TOKEN_EQUAL_OP:
                    case TOKEN_EQUAL_KW:
                        emitByte(compiler, OP_EQUAL);
                        goto END_PARSE_GROUPING;
                    case TOKEN_NOT_EQUAL_OP:
                    case TOKEN_NOT_EQUAL_KW:
                        emitByte(compiler, OP_EQUAL);
                        emitByte(compiler, OP_NOT);
                        goto END_PARSE_GROUPING;
                    case TOKEN_GREATER_OP:
                    case TOKEN_GREATER_KW:
                        emitByte(compiler, OP_GREATER);
                        goto END_PARSE_GROUPING;
                    case TOKEN_GREATER_EQUAL_OP:
                    case TOKEN_GREATER_EQUAL_KW:
                        emitByte(compiler, OP_LESS);
                        emitByte(compiler, OP_NOT);
                        goto END_PARSE_GROUPING;
                    case TOKEN_LESS_OP:
                    case TOKEN_LESS_KW:
                        emitByte(compiler, OP_LESS);
                        goto END_PARSE_GROUPING;
                    case TOKEN_LESS_EQUAL_OP:
                    case TOKEN_LESS_EQUAL_KW:
                        emitByte(compiler, OP_GREATER);
                        emitByte(compiler, OP_NOT);
                        goto END_PARSE_GROUPING;
                    default:
                        compiler->parser->hadError = true;
                        ERROR_LOG(
                            "[line %d] Error: Unknown operator in expression",
                            compiler->parser->current.line);
                        return;
                }
            }
            break;
        }
        default: {
            // A grouping is either a function call or a block of 1+
            // expressions. The disambiguation strategy is the following:
            // 1. If the next token is an identifier, it is a function call.
            // 2. If the next token is an open parenthesis and the next after is
            // a FN keyword,
            //   it is a function call with an anonymous function as the callee.
            // 3. Otherwise, it's a block.
            // if (compiler->parser->current.type == TOKEN_IDENTIFIER) {
            //    parseFunctionCall(compiler);
            //} else {
            //    parseBlock(compiler);
            //}
            // if (compiler->parser->hadError) return;
            // break;

            switch (compiler->parser->current.type) {
                case TOKEN_IDENTIFIER:
                    break;  // It's a function call, we will parse it below
                case TOKEN_LPAREN:
                    if (compiler->parser->next.type == TOKEN_FN_KW) {
                        break;  // It's a function call with an anonymous function callee
                    }
                    // Otherwise, it's a block
                default:
                    parseBlock(compiler);
                    return;
            }

            parseExpression(compiler);
            int arg_count = 0;
            while (compiler->parser->current.type != TOKEN_RPAREN) {
                if (arg_count > MAX_ARITY) {
                    compiler->parser->hadError = true;
                    ERROR_LOG(
                        "[line %d] Error: Too many arguments in a function "
                        "call",
                        compiler->parser->current.line);
                    return;
                }
                parseExpression(compiler);
                if (compiler->parser->hadError) return;
                arg_count++;
            }
            emitBytes(compiler, OP_CALL, (uint8_t)arg_count);
            break;
        }
    }

END_PARSE_GROUPING:
    consume(compiler, TOKEN_RPAREN, "expect ')' after expression");
}

static void namedVariable(Compiler* compiler, Token name) {
    // Try local lookup first
    int arg = resolveLocal(compiler, name);
    if (arg != -1) {
        emitBytes(compiler, OP_GET_LOCAL, (uint8_t)arg);
        return;
    }
    // Fall back to global lookup
    int const_index = identifierConstant(compiler, name);

    if (const_index > UINT16_MAX) {
        compiler->parser->hadError = true;
        ERROR_LOG("[line %d] Error: Too many constants in one chunk",
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
            ERROR_LOG("[line %d] Error: Expected expression",
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
    push(vm, OBJ_VAL(compiler.function));

    advance(&compiler);

#define WILL_READ_BODY() (compiler.parser->current.type != TOKEN_EOF)

    do {
        parseExpression(&compiler);
        if (compiler.parser->hadError) break;
        if (WILL_READ_BODY()) {
            emitByte(&compiler, OP_POP);
        }
    } while (WILL_READ_BODY());

#undef WILL_READ_BODY

    if (!compiler.parser->hadError) {
        consume(&compiler, TOKEN_EOF, "expect the end of expression");
    }

    ObjFunction* function = endCompiler(&compiler);

    return parser.hadError ? NULL : function;
}
