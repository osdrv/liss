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

static bool isAtEnd(Scanner* scanner) { return *(scanner->current) == '\0'; }

static char peek(Scanner* scanner) { return *(scanner->current); }

static char peekNext(Scanner* scanner) {
    if (isAtEnd(scanner)) return '\0';
    return *(scanner->current + 1);
}

static char prev(Scanner* scanner) {
    if (scanner->current == scanner->start) {
        return '\0';  // No previous character
    }
    return *(scanner->current - 1);
}

static bool isDigit(Scanner* scanner) {
    return peek(scanner) >= '0' && peek(scanner) <= '9';
}

static bool isHexDigit(Scanner* scanner) {
    char c = peek(scanner);
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static bool isAlpha(Scanner* scanner) {
    char c = peek(scanner);
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isAnyChar(Scanner* scanner, const char* options) {
    char c = peek(scanner);
    for (int i = 0; options[i] != '\0'; i++) {
        if (c == options[i]) {
            return true;
        }
    }
    return false;
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

static TokenType checkKeyword(Scanner* scanner, int start, int length,
                              const char* rest, TokenType type) {
    if (scanner->current - scanner->start == start + length &&
        memcmp(scanner->start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static TokenType identifierType(Scanner* scanner) {
    switch (scanner->start[0]) {
        case 'a':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'n':
                        return checkKeyword(scanner, 2, 1, "d", TOKEN_AND);
                    case 's':
                        return checkKeyword(scanner, 2, 0, "", TOKEN_AS);
                    default:
                        break;
                }
            }
        case 'b':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a':
                        return checkKeyword(scanner, 2, 2, "nd", TOKEN_BAND);
                    case 'n':
                        return checkKeyword(scanner, 2, 2, "ot", TOKEN_BNOT);
                    case 'o':
                        return checkKeyword(scanner, 2, 1, "r", TOKEN_BOR);
                    case 'r':
                        return checkKeyword(scanner, 2, 8, "eakpoint",
                                            TOKEN_BREAKPOINT);
                    case 's':
                        if (scanner->current - scanner->start > 2) {
                            switch (scanner->start[2]) {
                                case 'l':
                                    return TOKEN_LSHIFT;
                                case 'r':
                                    return TOKEN_RSHIFT;
                                default:
                                    break;
                            }
                        }
                    case 'x':
                        return checkKeyword(scanner, 2, 2, "or", TOKEN_BXOR);
                    default:
                        break;
                }
            }
        case 'c':
            return checkKeyword(scanner, 1, 3, "ond", TOKEN_COND);
        case 'd':
            return checkKeyword(scanner, 1, 2, "iv", TOKEN_SLASH);
        case 'e':
            return checkKeyword(scanner, 1, 1, "q", TOKEN_EQUAL);
        case 'f':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a':
                        return checkKeyword(scanner, 2, 3, "lse", TOKEN_FALSE);
                    case 'n':
                        return checkKeyword(scanner, 2, 0, "", TOKEN_FN);
                    default:
                        break;
                }
            }
        case 'g':
            if (scanner->current - scanner->start > 1) {
                if (scanner->current - scanner->start > 2) {
                    return checkKeyword(scanner, 1, 2, "te",
                                        TOKEN_GREATER_EQUAL);
                }
                return checkKeyword(scanner, 1, 1, "t", TOKEN_GREATER);
            }
        case 'i':
            return checkKeyword(scanner, 1, 5, "mport", TOKEN_IMPORT);
        case 'l':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'e':
                        return checkKeyword(scanner, 2, 1, "t", TOKEN_LET);
                    case 't':
                        if (scanner->current - scanner->start > 2) {
                            return checkKeyword(scanner, 2, 1, "e",
                                                TOKEN_LESS_EQUAL);
                        }
                        return checkKeyword(scanner, 2, 0, "", TOKEN_LESS);
                    default:
                        break;
                }
            }
        case 'm':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'i':
                        return checkKeyword(scanner, 2, 3, "nus", TOKEN_MINUS);
                    case 'o':
                        return checkKeyword(scanner, 2, 1, "d", TOKEN_MODULO);
                    case 'u':
                        return checkKeyword(scanner, 2, 2, "lt", TOKEN_STAR);
                    default:
                        break;
                }
            }
        case 'n':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'e':
                        return checkKeyword(scanner, 2, 0, "", TOKEN_NOT_EQUAL);
                    case 'o':
                        return checkKeyword(scanner, 2, 1, "t", TOKEN_NOT);
                    case 'u':
                        return checkKeyword(scanner, 2, 2, "ll", TOKEN_NIL);
                    default:
                        break;
                }
            }
        case 'o':
            return checkKeyword(scanner, 1, 1, "r", TOKEN_OR);
        case 'p':
            return checkKeyword(scanner, 1, 3, "lus", TOKEN_PLUS);
        case 'r':
            return checkKeyword(scanner, 1, 5, "aise!", TOKEN_RAISE);
        case 's':
            return checkKeyword(scanner, 1, 5, "witch", TOKEN_SWITCH);
        case 't':
            if ((scanner->current - scanner->start > 1) &&
                scanner->start[1] == 'r') {
                if (scanner->current - scanner->start > 2) {
                    switch (scanner->start[2]) {
                        case 'u':
                            return checkKeyword(scanner, 3, 1, "e", TOKEN_TRUE);
                        case 'y':
                            return checkKeyword(scanner, 3, 0, "", TOKEN_TRY);
                        default:
                            break;
                    }
                }
            }
    }
    return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner* scanner) {
    while (
        isAlpha(scanner) || isDigit(scanner) ||
        ((scanner->current - scanner->start > 0) && isAnyChar(scanner, "?!")))
        advance(scanner);
    return mkToken(scanner, identifierType(scanner));
}

static Token number(Scanner* scanner) {
    while (isDigit(scanner)) advance(scanner);

    switch (peek(scanner)) {
        case '.':
            advance(scanner);
            goto fraction;
        case 'e':
        case 'E':
            advance(scanner);
            goto exponent;
        case 'x':
            advance(scanner);
            while (isHexDigit(scanner)) advance(scanner);
            goto result;
        default:
            goto result;
    }

fraction:
    while (isDigit(scanner)) advance(scanner);
    // do not jump and check for exponent if there is no digits after dot

exponent:
    if (peek(scanner) == 'e' || peek(scanner) == 'E') {
        advance(scanner);
        if (peek(scanner) == '+' || peek(scanner) == '-') {
            advance(scanner);
        }
        while (isDigit(scanner)) advance(scanner);
    }

result:
    return mkToken(scanner, TOKEN_NUMBER);
}

Token scanToken(Scanner* scanner) {
    eatWhiteSpace(scanner);

    scanner->start = scanner->current;

    if (isAtEnd(scanner)) return mkToken(scanner, TOKEN_EOF);

    if (isAlpha(scanner)) return identifier(scanner);
    if (isDigit(scanner)) return number(scanner);

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
            if (peekNext(scanner) >= '0' && peekNext(scanner) <= '9') {
                advance(scanner);
                return number(scanner);
            }
            return mkToken(scanner, TOKEN_PLUS);
        case '-':
            if (peekNext(scanner) >= '0' && peekNext(scanner) <= '9') {
                advance(scanner);
                return number(scanner);
            }
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
                return mkToken(scanner, TOKEN_EQUAL);
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
