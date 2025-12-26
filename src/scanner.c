#include "scanner.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"

void initScanner(Scanner* scanner, const char* source) {
    scanner->start = source;
    scanner->current = source;
    scanner->line = 1;
}

static char advance(Scanner* scanner) {
    scanner->current++;
    return scanner->current[-1];
}

bool isAtEnd(Scanner* scanner) { return *(scanner->current) == '\0'; }

char peek(Scanner* scanner) { return *(scanner->current); }

char prev(Scanner* scanner) {
    if (scanner->current == scanner->start) {
        return '\0';  // No previous character
    }
    return *(scanner->current - 1);
}

bool isDigit(Scanner* scanner) {
    return peek(scanner) >= '0' && peek(scanner) <= '9';
}

bool isAlpha(Scanner* scanner) {
    char c = peek(scanner);
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

void eatWhitespace(Scanner* scanner) {
    while (!isAtEnd(scanner)) {
        char c = peek(scanner);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(scanner);
                break;
            case '\n':
                scanner->line++;
                advance(scanner);
                break;
            default:
                return;
        }
    }
}

static Token mkToken(Scanner* scanner, TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner->start;
    token.length = (int)(scanner->current - scanner->start);
    token.line = scanner->line;
    return token;
}

static Token errToken(Scanner* scanner, const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner->line;
    return token;
}

static void eatWhiteSpace(Scanner* scanner) {
    for (;;) {
        char c = peek(scanner);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(scanner);
                break;
            case '\n':
                scanner->line++;
                advance(scanner);
                break;
            case ';':
                // A comment goes until the end of the line.
                while (peek(scanner) != '\n' && !isAtEnd(scanner)) {
                    advance(scanner);
                }
                break;
            default:
                return;
        }
    }
}

Token scanToken(Scanner* scanner) {
    eatWhiteSpace(scanner);

    scanner->start = scanner->current;

    if (isAtEnd(scanner)) return mkToken(scanner, TOKEN_EOF);

    char c = advance(scanner);
    switch (c) {
        case '(':
            return mkToken(scanner, TOKEN_LPAREN);
        case ')':
            return mkToken(scanner, TOKEN_RPAREN);
        case '[':
            return mkToken(scanner, TOKEN_LBRAKET);
        case ']':
            return mkToken(scanner, TOKEN_RBRAKET);
        case '+':
            return mkToken(scanner, TOKEN_PLUS);
        case '-':
            return mkToken(scanner, TOKEN_MINUS);
        case '*':
            return mkToken(scanner, TOKEN_STAR);
        case '/':
            return mkToken(scanner, TOKEN_SLASH);
        case '%':
            return mkToken(scanner, TOKEN_MODULO);
        case '!':
            if (peek(scanner) == '=') {
                advance(scanner);
                return mkToken(scanner, TOKEN_NOT_EQUAL);
            } else {
                return mkToken(scanner, TOKEN_NOT);
            }
        case '=':
            if (peek(scanner) == '=') {
                advance(scanner);
                return mkToken(scanner, TOKEN_EQUAL);
            } else {
                return mkToken(scanner, TOKEN_ACCESSOR);
            }
        case '<':
            if (peek(scanner) == '=') {
                advance(scanner);
                return mkToken(scanner, TOKEN_LESS_EQUAL);
            } else if (peek(scanner) == '<') {
                advance(scanner);
                return mkToken(scanner, TOKEN_LSHIFT);
            } else {
                return mkToken(scanner, TOKEN_LESS);
            }
        case '>':
            if (peek(scanner) == '=') {
                advance(scanner);
                return mkToken(scanner, TOKEN_GREATER_EQUAL);
            } else if (peek(scanner) == '>') {
                advance(scanner);
                return mkToken(scanner, TOKEN_RSHIFT);
            } else {
                return mkToken(scanner, TOKEN_GREATER);
            }
        case '&':
            if (peek(scanner) == '&') {
                advance(scanner);
                return mkToken(scanner, TOKEN_AND);
            } else {
                return mkToken(scanner, TOKEN_BAND);
            }
        case '^':
            return mkToken(scanner, TOKEN_BXOR);
        case '|':
            if (peek(scanner) == '|') {
                advance(scanner);
                return mkToken(scanner, TOKEN_OR);
            } else {
                return mkToken(scanner, TOKEN_BOR);
            }
        case '~':
            return mkToken(scanner, TOKEN_BNOT);
    }

    return errToken(scanner, "Unexpected character.");
}
