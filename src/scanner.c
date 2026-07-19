#include "scanner.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"

#define STRING_LITERAL_INIT_BUF_SIZE 16
#define ASCII_SIZE 128

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

static char peekNext(Scanner* scanner) { return *(scanner->current + 1); }

// A hyphen is valid mid-identifier (Lisp convention) when followed by a letter.
static bool isMidHyphen(Scanner* scanner) {
    if (peek(scanner) != '-') return false;
    char next = peekNext(scanner);
    return (next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') ||
           next == '_';
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

// This function returns true if the current character is in the options string
// and hasn't been seen before in the current token. It uses a boolean array
// to track which characters have been seen.
static bool isAnyCharOnce(Scanner* scanner, const char* options,
                          unsigned char seen_chars[ASCII_SIZE]) {
    char c = peek(scanner);
    for (const char* opt = options; *opt != '\0'; ++opt) {
        if (*opt == c && !(seen_chars[*opt] & 1)) {
            // Set the flag for this character and return true.
            seen_chars[*opt] |= 1;
            return true;
        }
    }
    return false;
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
    bool seen_chars[ASCII_SIZE] = {0};
    while (
        isAlpha(scanner) || isDigit(scanner) ||
        ((scanner->current - scanner->start > 0) &&
         (isAnyCharOnce(scanner, "?!:", seen_chars) || isMidHyphen(scanner))))
        advance(scanner);
    TokenType type = identifierType(scanner);
    return mkToken(scanner, type);
}

static Token number(Scanner* scanner) {
    bool is_real = false;
    while (isDigit(scanner)) advance(scanner);

    // Hex numbers start with 0x or 0X and contain hex digits only (no exponent)
    if (peek(scanner) == 'x' || peek(scanner) == 'X') {
        advance(scanner);
        while (isHexDigit(scanner)) advance(scanner);
        return mkToken(scanner, TOKEN_INT);
    }

    if (peek(scanner) == '.') {
        is_real = true;
        advance(scanner);
        while (isDigit(scanner)) advance(scanner);
    }

    if (peek(scanner) == 'e' || peek(scanner) == 'E') {
        is_real = true;
        advance(scanner);
        if (peek(scanner) == '+' || peek(scanner) == '-') {
            advance(scanner);
        }
        while (isDigit(scanner)) advance(scanner);
    }

    return mkToken(scanner, is_real ? TOKEN_REAL : TOKEN_INT);
}

static Token string(Scanner* scanner) {
    size_t bptr = 0;
    size_t bufsize = STRING_LITERAL_INIT_BUF_SIZE;
    char* buf = malloc(bufsize);
    bool escape = false;

    while (!isAtEnd(scanner)) {
        char c = advance(scanner);
        if (bptr + 1 >= bufsize) {
            bufsize *= 2;
            buf = realloc(buf, bufsize);
        }

        if (escape) {
            switch (c) {
                case 'n':
                    buf[bptr++] = '\n';
                    break;
                case 't':
                    buf[bptr++] = '\t';
                    break;
                case 'r':
                    buf[bptr++] = '\r';
                    break;
                case '"':
                    buf[bptr++] = '"';
                    break;
                case '\\':
                    buf[bptr++] = '\\';
                    break;
                default:
                    buf[bptr++] = c;  // Unknown escape, just add the char
                    break;
            }
            escape = false;
        } else {
            if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                // End of string
                buf[bptr] = '\0';
                Token token = mkToken(scanner, TOKEN_STRING);
                token.start = buf;  // Set the new processed string
                token.length = bptr;
                return token;
            } else {
                buf[bptr++] = c;
            }
        }
    }

    free(buf);
    return errToken(scanner, "Unterminated string.");
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
        case '.':
            return mkToken(scanner, TOKEN_DOT);
        case '+':
            return mkToken(scanner, TOKEN_PLUS_OP);
        case '-':
            if (peek(scanner) == '>') {
                advance(scanner);
                return mkToken(scanner, TOKEN_ARROW_KW);
            }
            if (isDigit(scanner)) return number(scanner);
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
