#ifndef liss_scanner_h
#define liss_scanner_h

#include <stddef.h>

#include "token.h"

typedef struct {
    const char* start;
    const char* current;
    int line;
} Scanner;

typedef struct {
    const char* name;
    int length;
    TokenType type;
} Keyword;

static Keyword keywords[] = {
    {"and", 3, TOKEN_AND},       {"as", 2, TOKEN_AS},
    {"band", 4, TOKEN_BAND},     {"bnot", 4, TOKEN_BNOT},
    {"bor", 3, TOKEN_BOR},       {"breakpoint", 10, TOKEN_BREAKPOINT},
    {"bsl", 3, TOKEN_LSHIFT},    {"bsr", 3, TOKEN_RSHIFT},
    {"bxor", 4, TOKEN_BXOR},     {"cond", 4, TOKEN_COND},
    {"div", 3, TOKEN_SLASH},     {"eq", 2, TOKEN_EQUAL},
    {"false", 5, TOKEN_FALSE},   {"fn", 2, TOKEN_FN},
    {"gt", 2, TOKEN_GREATER},    {"gte", 3, TOKEN_GREATER_EQUAL},
    {"import", 6, TOKEN_IMPORT}, {"let", 3, TOKEN_LET},
    {"lt", 2, TOKEN_LESS},       {"lte", 3, TOKEN_LESS_EQUAL},
    {"mod", 3, TOKEN_MODULO},    {"mul", 3, TOKEN_STAR},
    {"ne", 2, TOKEN_NOT_EQUAL},  {"not", 3, TOKEN_NOT},
    {"null", 4, TOKEN_NULL},     {"or", 2, TOKEN_OR},
    {"raise!", 6, TOKEN_RAISE},  {"switch", 6, TOKEN_SWITCH},
    {"true", 4, TOKEN_TRUE},     {"try", 3, TOKEN_TRY},
};

void initScanner(Scanner* scanner, const char* source);

Token scanToken(Scanner* scanner);

#endif
