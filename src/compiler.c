#include "compiler.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "gc.h"
#include "object.h"
#include "opcode.h"
#include "token.h"
#include "vm.h"

static void parseExpression(Compiler* compiler, bool is_tail);

static void initParser(Parser* parser) {
    parser->hadError = false;
    parser->panicMode = false;
    parser->previous = (Token){0};
    parser->current = (Token){0};
    parser->next = (Token){0};
}

// Function advance moves the parser forward.
// In case this is the first read, it will read two tokens to fill both current
// and next. If the current token is an error, it will report it and keep
// advancing until it finds a non-error token or reaches the end of the input.
static void advance(Compiler* compiler) {
    Parser* parser = compiler->parser;

    parser->previous = parser->current;
    parser->current = parser->next;

    for (;;) {
        parser->current = parser->next;
        parser->next = scanToken(&parser->scanner);
        // If the current token is TOKEN_ZERO, read one more token as it wasn't
        // read yet.
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

static Token consumeAnyOf(Compiler* compiler, size_t count, TokenType types[],
                          const char* message) {
    Parser* parser = compiler->parser;
    for (size_t i = 0; i < count; i++) {
        if (parser->current.type == types[i]) {
            Token token = parser->current;
            advance(compiler);
            return token;
        }
    }

    parser->hadError = true;
    ERROR_LOG("[line %d] Error: %s", parser->current.line, message);
    (void)message;  // Suppress unused parameter warning

    return parser->current;
}

static Chunk* currentChunk(Compiler* compiler) {
    return &compiler->function->chunk;
}

static void emitByte(Compiler* compiler, uint8_t byte) {
    writeChunk(compiler->vm, currentChunk(compiler), byte);
}

static void emitBytes(Compiler* compiler, uint8_t byte1, uint8_t byte2) {
    emitByte(compiler, byte1);
    emitByte(compiler, byte2);
}

static void emitConstant(Compiler* compiler, Value value) {
    Chunk* chunk = currentChunk(compiler);
    int constant = addConstant(compiler->vm, chunk, value);
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

static void maybePatchTailCall(Compiler* compiler) {
    Chunk* chunk = currentChunk(compiler);
    if (chunk->count >= 2 && chunk->code[chunk->count - 2] == OP_CALL) {
        chunk->code[chunk->count - 2] = OP_TAIL_CALL;
    }
}

static void initCompiler(Compiler* compiler, Compiler* enclosing,
                         ObjModule* module) {
    // Enforce that the module is not NULL. Unconditionally.
    if (module == NULL) {
        ERROR_LOG("Internal error: Compiler requires a non-NULL module.");
        exit(1);
    }

    compiler->enclosing = enclosing;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->module = module;

    if (enclosing != NULL) {
        compiler->parser = enclosing->parser;
        compiler->vm = enclosing->vm;
    }
    compiler->function = NULL;
    initTable(&compiler->aliases);

    Local* local = &compiler->locals[compiler->local_count++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;

    compiler->upvalue_cnt = 0;
    compiler->function = newFunction(compiler->vm, compiler->module);
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
    Token prev = compiler->parser->previous;
    char* buf = (char*)malloc(prev.length + 1);
    if (buf == NULL) {
        compiler->parser->hadError = true;
        ERROR_LOG(
            "[line %d] Error: Memory allocation failed for number literal",
            compiler->parser->previous.line);
        return;
    }
    memcpy(buf, prev.start, prev.length);
    buf[prev.length] = '\0';

    if (prev.type == TOKEN_INT) {
        int64_t value = strtoll(prev.start, NULL,
                                0);  // Support hex, octal, and binary literals
        if (errno == ERANGE) {
            compiler->parser->hadError = true;
            ERROR_LOG("[line %d] Error: Integer literal out of range",
                      compiler->parser->previous.line);
            goto END_PARSE_NUMBER;
        }
        emitConstant(compiler, INT_VAL(value));
    } else {
        double value = strtod(compiler->parser->previous.start, NULL);
        if (errno == ERANGE) {
            compiler->parser->hadError = true;
            ERROR_LOG("[line %d] Error: Real number literal out of range",
                      compiler->parser->previous.line);
            goto END_PARSE_NUMBER;
        }
        emitConstant(compiler, REAL_VAL(value));
    }
END_PARSE_NUMBER:
    free(buf);
}

static void parseString(Compiler* compiler) {
    Token* token = &compiler->parser->previous;
    ObjString* string = copyString(compiler->vm, token->start, token->length);
    emitConstant(compiler, OBJ_VAL(string));
}

static void parseAnd(Compiler* compiler) {
    int jump_list[100];
    int jump_count = 0;

    parseExpression(compiler, false);
    if (compiler->parser->hadError) return;

    while (compiler->parser->current.type != TOKEN_RPAREN) {
        int jump = emitJump(compiler, OP_JUMP_IF_FALSE);
        jump_list[jump_count++] = jump;
        parseExpression(compiler, false);
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

    parseExpression(compiler, false);
    if (compiler->parser->hadError) return;

    while (compiler->parser->current.type != TOKEN_RPAREN) {
        // If the previous expression is false, jump to the next operand
        int else_jump = emitJump(compiler, OP_JUMP_IF_FALSE);
        int end_jump = emitJump(compiler, OP_JUMP);
        jump_list[jump_count++] = end_jump;
        patchJump(compiler, else_jump);
        emitByte(compiler, OP_POP);
        parseExpression(compiler, false);
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

static int addUpvalue(Compiler* compiler, uint8_t index, bool is_local) {
    int cnt = compiler->upvalue_cnt;
    for (int i = 0; i < cnt; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->is_local == is_local) {
            return i;
        }
    }
    if (cnt == MAX_UPVALUES) {
        ERROR_LOG("Too many closure variables in function.");
        return -1;
    }
    compiler->upvalues[cnt].is_local = is_local;
    compiler->upvalues[cnt].index = index;
    compiler->function->upvalue_cnt = cnt + 1;
    return compiler->upvalue_cnt++;
}

static int resolveUpvalue(Compiler* compiler, Token name) {
    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static int identifierConstant(Compiler* compiler, Token name) {
    ObjString* var_name = copyString(compiler->vm, name.start, name.length);
    return addConstant(compiler->vm, &compiler->function->chunk,
                       OBJ_VAL(var_name));
}

static bool identifiersEqual(Token* a, Token* b) {
    return a->length == b->length && memcmp(a->start, b->start, a->length) == 0;
}

static void parseLet(Compiler* compiler) {
    Token identifier =
        consume(compiler, TOKEN_IDENTIFIER, "expect an identifier after `let`");
    if (compiler->parser->hadError) return;

    parseExpression(compiler, false);
    if (compiler->parser->hadError) return;

    if (compiler->scope_depth == 0) {
        // Global variable declaration
        int var_index = identifierConstant(compiler, identifier);
        Value name = currentChunk(compiler)->constants.values[var_index];
        if (tableGet(&compiler->module->symbols, name) != NULL) {
            compiler->parser->hadError = true;
            ERROR_LOG(
                "[line %d] Error: Cannot redeclare global variable '%.*s'",
                identifier.line, identifier.length, identifier.start);
            return;
        }
        if (compiler->added_globals_cnt >= MAX_GLOBALS) {
            compiler->parser->hadError = true;
            ERROR_LOG(
                "[line %d] Error: Too many global variables declared in "
                "program",
                identifier.line);
            return;
        }
        tableInsert(&compiler->module->symbols, name, NIL_VAL);
        compiler->added_globals[compiler->added_globals_cnt++] = name;
        emitByte(compiler, OP_SET_GLOBAL);
        emitBytes(compiler, (uint8_t)(var_index >> 8),
                  (uint8_t)(var_index & 0xff));
    } else {
        // Local variable declaration
        for (int i = compiler->local_count - 1; i >= 0; i--) {
            Local* local = &compiler->locals[i];
            if (local->depth != -1 && local->depth < compiler->scope_depth) {
                break;
            }
            if (identifiersEqual(&identifier, &local->name)) {
                compiler->parser->hadError = true;
                ERROR_LOG(
                    "[line %d] Error: Cannot redeclare variable '%.*s' in this "
                    "scope",
                    identifier.line, identifier.length, identifier.start);
                return;
            }
        }
        addLocal(compiler, identifier);
    }
}

static void parseCond(Compiler* compiler, bool is_tail) {
    // Parse condition
    parseExpression(compiler, false);
    if (compiler->parser->hadError) return;
    int else_jump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);

    // Parse then branch
    parseExpression(compiler, is_tail);
    if (compiler->parser->hadError) return;
    int end_jump = emitJump(compiler, OP_JUMP);
    patchJump(compiler, else_jump);
    emitByte(compiler, OP_POP);

    // Parse else branch
    if (compiler->parser->current.type != TOKEN_RPAREN) {
        parseExpression(compiler, is_tail);
        if (compiler->parser->hadError) return;
    } else {
        // If there's no else branch, we emit a null value
        emitByte(compiler, OP_NULL);
    }
    patchJump(compiler, end_jump);
}

static ObjFunction* compileFunction(Compiler* compiler, Compiler* fn_compiler) {
    initCompiler(fn_compiler, compiler, compiler->module);

    push(compiler->vm, OBJ_VAL(fn_compiler->function));

    fn_compiler->scope_depth = compiler->scope_depth + 1;

    if (fn_compiler->parser->current.type == TOKEN_IDENTIFIER) {
        Token fn_name = consume(fn_compiler, TOKEN_IDENTIFIER,
                                "expect function name after 'fn'");
        fn_compiler->function->name =
            copyString(fn_compiler->vm, fn_name.start, fn_name.length);
    } else {
        fn_compiler->function->name = NULL;
    }

    consume(fn_compiler, TOKEN_LBRAKET, "expect '[' for function parameters");

    if (fn_compiler->parser->current.type != TOKEN_RBRAKET) {
        do {
            fn_compiler->function->arity++;
            if (fn_compiler->function->arity >= MAX_LOCALS) {
                compiler->parser->hadError = true;
                ERROR_LOG(
                    "[line %d] Error: max function parameter limit reached",
                    fn_compiler->parser->current.line);
                return NULL;
            }
            Token param =
                consume(fn_compiler, TOKEN_IDENTIFIER, "Expect parameter name");
            if (fn_compiler->parser->hadError) return NULL;
            addLocal(fn_compiler, param);
        } while (fn_compiler->parser->current.type == TOKEN_IDENTIFIER);
    }

    consume(fn_compiler, TOKEN_RBRAKET, "Expect ']' after parameters");

#define WILL_READ_BODY()                                  \
    (fn_compiler->parser->current.type != TOKEN_RPAREN && \
     fn_compiler->parser->current.type != TOKEN_ZERO &&   \
     fn_compiler->parser->current.type != TOKEN_EOF)

    bool is_empty_body = true;
    while (WILL_READ_BODY()) {
        parseExpression(fn_compiler, false);
        if (fn_compiler->parser->hadError) return NULL;
        is_empty_body = false;
        // We compare init_chunk_count to the current chunk count to determine
        // if the expression we just parsed emitted any bytecode. If it didn't,
        // we don't need to emit a POP instruction.
        if (WILL_READ_BODY()) {
            emitByte(fn_compiler, OP_POP);
        } else {
            // If this is the last expression in the function body, we check if
            // it's a tail call and emit a return if it isn't.
            maybePatchTailCall(fn_compiler);
        }
    }
    if (is_empty_body) {
        // If the function body is empty, we emit a null value so it returns nil
        emitByte(fn_compiler, OP_NULL);
    }

#undef WILL_READ_BODY

    ObjFunction* function = endCompiler(fn_compiler);
    return function;
}

static void parsePairOrBlock(Compiler* compiler, bool is_tail) {
    beginScope(compiler);
    bool first_expr = true;
    while (compiler->parser->current.type != TOKEN_RPAREN) {
        parseExpression(compiler, false);
        if (compiler->parser->hadError) return;
        if (first_expr && compiler->parser->current.type == TOKEN_DOT) {
            consume(compiler, TOKEN_DOT, "expect `.` when initializing a pair");
            parseExpression(compiler, false);
            if (compiler->parser->hadError) return;
            emitByte(compiler, OP_PAIR);
            break;
        }
        first_expr = false;
        if (compiler->parser->current.type != TOKEN_RPAREN) {
            emitByte(compiler, OP_POP);
        } else if (is_tail) {
            maybePatchTailCall(compiler);
        }
    }
    endScope(compiler);
}

static void parseTry(Compiler* compiler) {
    int jump_to = emitJump(compiler, OP_TRY_START);
    parseExpression(compiler, false);
    if (compiler->parser->hadError) return;
    emitByte(compiler, OP_TRY_END);
    patchJump(compiler, jump_to);
}

static void parseList(Compiler* compiler) {
    int len = 0;
    while (compiler->parser->current.type != TOKEN_RBRAKET) {
        parseExpression(compiler, false);
        if (compiler->parser->hadError) return;
        len++;
    }
    if (len > UINT8_MAX) {
        compiler->parser->hadError = true;
        ERROR_LOG("[line %d] Error: List literal too long",
                  compiler->parser->current.line);
        return;
    }
    consume(compiler, TOKEN_RBRAKET, "expect ']' after list literal");
    if (compiler->parser->hadError) return;
    emitBytes(compiler, OP_LIST, (uint8_t)(len & 0xff));
}

// Idk: looks clumsy, but useful.
static Token readStringOrIdentifier(Compiler* compiler, const char* error) {
    consumeAnyOf(compiler, 2, (TokenType[]){TOKEN_STRING, TOKEN_IDENTIFIER},
                 error);
    return compiler->parser->previous;
}

// There are 6 variations of import syntax:
// 1) (import "module_name")
// 2) (import module_name) -- equivalent to the one above, we keep the previous
// one for backward compatibility 3) (import module_name as alias) 4) (import
// "module_name" as alias) 5) (import module_name [foo bar baz]) 6) (import
// "module_name" ["foo" "bar" "baz"])
static void parseImport(Compiler* compiler) {
    Token module_name_token = readStringOrIdentifier(
        compiler, "expect module name as string or identifier");
    if (compiler->parser->hadError) {
        ERROR_LOG("[line %d] Error: expect module name as string or identifier",
                  compiler->parser->current.line);
        return;
    }
    ObjString* module_name_obj = copyString(
        compiler->vm, module_name_token.start, module_name_token.length);

    ObjModule* module = loadModule(compiler->vm, module_name_obj);
    if (module == NULL) {
        ERROR_LOG("[line %d] Error: could not load module %s",
                  compiler->parser->current.line, module_name_obj->chars);
    }

    if (compiler->parser->current.type == TOKEN_AS_KW) {
        advance(compiler);
        Token alias_token = readStringOrIdentifier(
            compiler, "expect alias as string or identifier");
        if (compiler->parser->hadError) {
            ERROR_LOG("[line %d] Error: expect alias as string or identifier",
                      compiler->parser->current.line);
            return;
        }
        ObjString* alias_obj =
            copyString(compiler->vm, alias_token.start, alias_token.length);
        // Insert alias -> module name mapping into the compiler's alias table.
        tableInsert(&compiler->aliases, OBJ_VAL(alias_obj),
                    OBJ_VAL(module_name_obj));
    } else {
        // If there is no alias, we insert module name -> module name mapping,
        // so we can resolve imports the same way.
        tableInsert(&compiler->aliases, OBJ_VAL(module_name_obj),
                    OBJ_VAL(module_name_obj));
    }

    if (compiler->parser->current.type == TOKEN_LBRAKET) {
        advance(compiler);
        Value* module_val =
            tableGet(&compiler->vm->modules, OBJ_VAL(module_name_obj));
        if (module_val == NULL) {
            compiler->parser->hadError = true;
            ERROR_LOG("[line %d] Error: module '%.*s' not found",
                      module_name_token.line, module_name_token.length,
                      module_name_token.start);
            return;
        }
        ObjModule* module = (ObjModule*)AS_MODULE(*module_val);
        while (compiler->parser->current.type != TOKEN_RBRAKET) {
            Token symbol_token = readStringOrIdentifier(
                compiler,
                "expect symbol name as string or identifier in import list");
            if (compiler->parser->hadError) {
                ERROR_LOG(
                    "[line %d] Error: expect symbol name as string or "
                    "identifier in import list",
                    compiler->parser->current.line);
                return;
            }
            ObjString* symbol_obj = copyString(compiler->vm, symbol_token.start,
                                               symbol_token.length);

            Value* remote_ptr = tableGet(&module->symbols, OBJ_VAL(symbol_obj));
            if (remote_ptr == NULL) {
                compiler->parser->hadError = true;
                ERROR_LOG(
                    "[line %d] Error: symbol '%.*s' not found in module '%.*s'",
                    symbol_token.line, symbol_token.length, symbol_token.start,
                    module_name_token.line, module_name_token.length,
                    module_name_token.start);
                return;
            }
            // NOTE: if I ever end up debugging expired module symbol pointers,
            // this place is the starting point. We are pointing at a location
            // in a hash map bucket, which could be reallocated and moved if
            // the table exceeded the "ought to be enogh for everyon" size.
            tableInsert(&compiler->module->imports, OBJ_VAL(symbol_obj),
                        *remote_ptr);
        }
        consume(compiler, TOKEN_RBRAKET, "expect `]` after the symbol list");
    }
    // Emit dangling NULL because each expression should return something
    // TODO: if I ever want to make modules first-level primitives (e.g. for
    // reflect) This would be the place to change: I would need to add a public
    // API for modules and return it as an object.
    emitByte(compiler, OP_NULL);
}

static void parseGrouping(Compiler* compiler, bool is_tail) {
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
            parseCond(compiler, is_tail);
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

            Compiler fn_compiler;
            ObjFunction* func = compileFunction(compiler, &fn_compiler);
            if (compiler->parser->hadError) return;
            if (is_named_fn) {
                func->name =
                    copyString(compiler->vm, fn_name.start, fn_name.length);
            }

            int arg = addConstant(compiler->vm, currentChunk(compiler),
                                  OBJ_VAL(func));
            emitByte(compiler, OP_CLOSURE);
            emitBytes(compiler, (uint8_t)(arg >> 8), (uint8_t)(arg & 0xff));
            for (int i = 0; i < func->upvalue_cnt; i++) {
                emitByte(compiler, fn_compiler.upvalues[i].is_local ? 1 : 0);
                emitByte(compiler, fn_compiler.upvalues[i].index);
            }

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
                    Value name =
                        currentChunk(compiler)->constants.values[var_name_ix];
                    tableInsert(&compiler->module->symbols, name, NIL_VAL);
                    emitByte(compiler, OP_SET_GLOBAL);
                    emitBytes(compiler, (uint8_t)(var_name_ix >> 8),
                              (uint8_t)(var_name_ix & 0xff));
                }
            }
            break;
        case TOKEN_TRY_KW:
            advance(compiler);
            parseTry(compiler);
            break;
        case TOKEN_IMPORT_KW:
            advance(compiler);
            parseImport(compiler);
            break;
        case TOKEN_NOT_OP:
        case TOKEN_NOT_KW:
            advance(compiler);
            parseExpression(compiler, is_tail);
            if (compiler->parser->hadError) return;
            emitByte(compiler, OP_NOT);
            break;
        case TOKEN_BNOT_OP:
        case TOKEN_BNOT_KW:
            advance(compiler);
            parseExpression(compiler, is_tail);
            if (compiler->parser->hadError) return;
            emitByte(compiler, OP_BNOT);
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
        case TOKEN_LESS_EQUAL_KW:
        case TOKEN_BAND_OP:
        case TOKEN_BAND_KW:
        case TOKEN_BOR_OP:
        case TOKEN_BOR_KW:
        case TOKEN_BXOR_OP:
        case TOKEN_BXOR_KW:
        case TOKEN_LSHIFT_OP:
        case TOKEN_LSHIFT_KW:
        case TOKEN_RSHIFT_OP:
        case TOKEN_RSHIFT_KW: {
            TokenType op = compiler->parser->current.type;
            advance(compiler);
            parseExpression(compiler, false);
            if (compiler->parser->hadError) return;

            while (compiler->parser->current.type != TOKEN_RPAREN) {
                parseExpression(compiler, false);
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
                    case TOKEN_BAND_OP:
                    case TOKEN_BAND_KW:
                        emitByte(compiler, OP_BAND);
                        goto END_PARSE_GROUPING;
                    case TOKEN_BOR_OP:
                    case TOKEN_BOR_KW:
                        emitByte(compiler, OP_BOR);
                        goto END_PARSE_GROUPING;
                    case TOKEN_BXOR_OP:
                    case TOKEN_BXOR_KW:
                        emitByte(compiler, OP_BXOR);
                        goto END_PARSE_GROUPING;
                    case TOKEN_LSHIFT_OP:
                    case TOKEN_LSHIFT_KW:
                        emitByte(compiler, OP_LSHIFT);
                        goto END_PARSE_GROUPING;
                    case TOKEN_RSHIFT_OP:
                    case TOKEN_RSHIFT_KW:
                        emitByte(compiler, OP_RSHIFT);
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
            // A grouping is either:
            //   - a function call, like: (foo 1 2 3), or (fn sq [n] (* n n) 10)
            //   - a pair: ("key" . "value")
            //   - a block of 1+ expressions
            // A grouping is either a function call or a block of 1+
            // expressions. The disambiguation strategy is the following:
            // 1. If the next token is an identifier, it is a function call.
            // 2. If the next token is an open parenthesis and the next after is
            // a FN keyword,
            //   it is a function call with an anonymous function as the callee.
            // 3. If the second token is TOKEN_DOT, it is a pair
            // 4. Otherwise, it's a block of expressions.
            switch (compiler->parser->current.type) {
                case TOKEN_IDENTIFIER:
                    break;  // It's a function call, we will parse it below
                case TOKEN_LPAREN:
                    if (compiler->parser->next.type == TOKEN_FN_KW) {
                        break;  // It's a function call with an anonymous
                                // function callee
                    }
                    // Otherwise, it's a block
                default:
                    parsePairOrBlock(compiler, false);
                    goto END_PARSE_GROUPING;
            }

            parseExpression(compiler, false);
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
                parseExpression(compiler, false);
                if (compiler->parser->hadError) return;
                arg_count++;
            }
            emitBytes(compiler, is_tail ? OP_TAIL_CALL : OP_CALL,
                      (uint8_t)arg_count);
            break;
        }
    }

END_PARSE_GROUPING:
    consume(compiler, TOKEN_RPAREN, "expect ')' after expression");
}

// Returns the index of the first occurrence of ch in str, or -1 if not found.
static int indexOf(const char* str, const size_t len, const char ch) {
    for (int i = 0; i < len; i++) {
        if (str[i] == ch) {
            return i;
        }
    }
    return -1;
}

static void namedVariable(Compiler* compiler, Token name) {
    // Check if the name contains ":". If it does, it is a module-qualified
    // name.
    int module_name_ix = indexOf(name.start, name.length, ':');
    if (module_name_ix != -1) {
        ObjString* raw_name =
            copyString(compiler->vm, name.start, module_name_ix);
        Compiler* current = compiler;
        Value* actual_name = NULL;
        while (current != NULL) {
            actual_name = tableGet(&current->aliases, OBJ_VAL(raw_name));
            if (actual_name != NULL) break;
            current = current->enclosing;
        }
        ObjString* module_name =
            (actual_name != NULL) ? AS_STRING(*actual_name) : raw_name;

        ObjString* var_name =
            copyString(compiler->vm, name.start + module_name_ix + 1,
                       name.length - module_name_ix - 1);
        int module_ix = addConstant(compiler->vm, currentChunk(compiler),
                                    OBJ_VAL(module_name));
        int var_ix = addConstant(compiler->vm, currentChunk(compiler),
                                 OBJ_VAL(var_name));
        emitByte(compiler, OP_GET_MODULE_GLOBAL);
        emitBytes(compiler, (uint8_t)(module_ix >> 8),
                  (uint8_t)(module_ix & 0xff));
        emitBytes(compiler, (uint8_t)(var_ix >> 8), (uint8_t)(var_ix & 0xff));
        return;
    }

    // Try local lookup first
    int arg = resolveLocal(compiler, name);
    if (arg != -1) {
        emitBytes(compiler, OP_GET_LOCAL, (uint8_t)arg);
        return;
    }

    // Try upvalues
    arg = resolveUpvalue(compiler, name);
    if (arg != -1) {
        emitBytes(compiler, OP_GET_UPVALUE, (uint8_t)arg);
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

static void parseExpression(Compiler* compiler, bool is_tail) {
    switch (compiler->parser->current.type) {
        case TOKEN_INT:
        case TOKEN_REAL:
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
            parseGrouping(compiler, is_tail);
            break;
        case TOKEN_IDENTIFIER:
            namedVariable(compiler, compiler->parser->current);
            advance(compiler);
            break;
        case TOKEN_MINUS_OP:
            // Unary minus
            advance(compiler);
            parseExpression(compiler, false);
            emitByte(compiler, OP_NEGATE);
            break;
        case TOKEN_LBRAKET:
            advance(compiler);
            parseList(compiler);
            break;
        default:
            compiler->parser->hadError = true;
            ERROR_LOG("[line %d] Error: Expected expression",
                      compiler->parser->current.line);
            break;
    }
}

void markCompilerRoots(VM* vm) {
    Compiler* compiler = (Compiler*)vm->compiler;
    while (compiler != NULL) {
        push(vm, OBJ_VAL(compiler->function));
        markObject(vm, (Obj*)compiler->function);
        markTable(vm, &compiler->aliases);
        pop(vm);
        compiler = compiler->enclosing;
    }
}

ObjFunction* compile(VM* vm, const char* source, ObjModule* module) {
    Parser parser;
    initParser(&parser);
    initScanner(&parser.scanner, source);

    Compiler compiler;
    compiler.vm = vm;
    compiler.parser = &parser;
    compiler.added_globals_cnt = 0;
    void* prev_compiler = vm->compiler;
    vm->compiler = &compiler;
    initCompiler(&compiler, NULL, module);
    push(vm, OBJ_VAL(compiler.function));

    advance(&compiler);

#define WILL_READ_BODY() (compiler.parser->current.type != TOKEN_EOF)

    do {
        parseExpression(&compiler, true);
        if (compiler.parser->hadError) break;
        if (WILL_READ_BODY()) {
            emitByte(&compiler, OP_POP);
        }
    } while (WILL_READ_BODY());

#undef WILL_READ_BODY

    if (compiler.parser->hadError) {
        for (int i = 0; i < compiler.added_globals_cnt; i++) {
            tableRemove(&compiler.function->module->symbols,
                        compiler.added_globals[i]);
        }
        goto END_COMPILE;
    }

    consume(&compiler, TOKEN_EOF, "expect the end of expression");
    ObjFunction* function = endCompiler(&compiler);

END_COMPILE:
    pop(vm);  // pop the compiler.function
    vm->compiler = prev_compiler;
    return parser.hadError ? NULL : function;
}
