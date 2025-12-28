#include "scanner.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"

// The implementation is inspired by Crafting Interpreters book by Bob Nystrom
// https://craftinginterpreters.com/scanning-on-demand.html

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

static TokenType identifierType(Scanner* scanner) {
    for (size_t i = 0; i < sizeof(keywords) / sizeof(Keyword); i++) {
        Keyword keyword = keywords[i];
        if (scanner->current - scanner->start == keyword.length &&
            memcmp(scanner->start, keyword.name, keyword.length) == 0) {
            return keyword.type;
        }
    }

    return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner* scanner) {
    while (
        isAlpha(scanner) || isDigit(scanner) ||
        ((scanner->current - scanner->start > 0) && isAnyChar(scanner, "?!")))
        advance(scanner);
    TokenType type = identifierType(scanner);
    return mkToken(scanner, type);
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

// This routime reads string literals as is. It does not process escape
// sequences.
static Token string(Scanner* scanner) {
    bool escape = false;
    while (!isAtEnd(scanner)) {
        char c = peek(scanner);
        switch (c) {
            case '"':
                if (!escape) {
                    advance(scanner);  // Consume the closing "
                    goto done;
                }
                escape = false;
                advance(scanner);
                break;
            case '\\':
                escape = !escape;
                advance(scanner);
                break;
            case '\n':
                scanner->line++;
                advance(scanner);
                break;
            default:
                escape = false;
                advance(scanner);
                break;
        }
    }
    return errToken(scanner, "Unterminated string.");
done:
    return mkToken(scanner, TOKEN_STRING);
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
            return mkToken(scanner, TOKEN_PLUS_OP);
        case '-':
            if (peekNext(scanner) >= '0' && peekNext(scanner) <= '9') {
                advance(scanner);
                return number(scanner);
            }
            return mkToken(scanner, TOKEN_MINUS_OP);
        case '*':
            return mkToken(scanner, TOKEN_STAR_OP);
        case '/':
            return mkToken(scanner, TOKEN_SLASH_OP);
        case '%':
            return mkToken(scanner, TOKEN_MODULO_OP);
        case '!':
            if (peek(scanner) == '=') {
                advance(scanner);
                return mkToken(scanner, TOKEN_NOT_EQUAL_OP);
            } else {
                return mkToken(scanner, TOKEN_NOT_OP);
            }
        case '=':
            if (peek(scanner) == '=') {
                advance(scanner);
                return mkToken(scanner, TOKEN_EQUAL_OP);
            } else {
                return mkToken(scanner, TOKEN_EQUAL_OP);
            }
        case '<':
            if (peek(scanner) == '=') {
                advance(scanner);
                return mkToken(scanner, TOKEN_LESS_EQUAL_OP);
            } else if (peek(scanner) == '<') {
                advance(scanner);
                return mkToken(scanner, TOKEN_LSHIFT_OP);
            } else {
                return mkToken(scanner, TOKEN_LESS_OP);
            }
        case '>':
            if (peek(scanner) == '=') {
                advance(scanner);
                return mkToken(scanner, TOKEN_GREATER_EQUAL_OP);
            } else if (peek(scanner) == '>') {
                advance(scanner);
                return mkToken(scanner, TOKEN_RSHIFT_OP);
            } else {
                return mkToken(scanner, TOKEN_GREATER_OP);
            }
        case '&':
            if (peek(scanner) == '&') {
                advance(scanner);
                return mkToken(scanner, TOKEN_BAND_OP);
            } else {
                return mkToken(scanner, TOKEN_AND_OP);
            }
        case '^':
            return mkToken(scanner, TOKEN_BXOR_OP);
        case '|':
            if (peek(scanner) == '|') {
                advance(scanner);
                return mkToken(scanner, TOKEN_BOR_OP);
            } else {
                return mkToken(scanner, TOKEN_OR_OP);
            }
        case '~':
            return mkToken(scanner, TOKEN_BNOT_OP);
        case '"':
            return string(scanner);
    }

    return errToken(scanner, "Unexpected character.");
}
