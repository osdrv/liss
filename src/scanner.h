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
    {"and", 3, TOKEN_AND_KW},       {"as", 2, TOKEN_AS_KW},
    {"band", 4, TOKEN_BAND_KW},     {"bnot", 4, TOKEN_BNOT_KW},
    {"bor", 3, TOKEN_BOR_KW},       {"breakpoint", 10, TOKEN_BREAKPOINT_KW},
    {"bsl", 3, TOKEN_LSHIFT_KW},    {"bsr", 3, TOKEN_RSHIFT_KW},
    {"bxor", 4, TOKEN_BXOR_KW},     {"cond", 4, TOKEN_COND_KW},
    {"div", 3, TOKEN_SLASH_KW},     {"eq", 2, TOKEN_EQUAL_KW},
    {"false", 5, TOKEN_FALSE_KW},   {"fn", 2, TOKEN_FN_KW},
    {"gt", 2, TOKEN_GREATER_KW},    {"gte", 3, TOKEN_GREATER_EQUAL_KW},
    {"import", 6, TOKEN_IMPORT_KW}, {"let", 3, TOKEN_LET_KW},
    {"lt", 2, TOKEN_LESS_KW},       {"lte", 3, TOKEN_LESS_EQUAL_KW},
    {"mod", 3, TOKEN_MODULO_KW},    {"mul", 3, TOKEN_STAR_KW},
    {"ne", 2, TOKEN_NOT_EQUAL_KW},  {"not", 3, TOKEN_NOT_KW},
    {"null", 4, TOKEN_NULL_KW},     {"or", 2, TOKEN_OR_KW},
    {"switch", 6, TOKEN_SWITCH_KW}, {"true", 4, TOKEN_TRUE_KW},
    {"try", 3, TOKEN_TRY_KW},
};

void initScanner(Scanner* scanner, const char* source);

Token scanToken(Scanner* scanner);

#endif
