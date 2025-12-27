#include "scanner.h"

#include "minunit.h"
#include "token.h"

static char* test_scanner_whitespace(void) {
    const char* source = "   \n\t  ";
    Scanner scanner;
    initScanner(&scanner, source);

    Token token = scanToken(&scanner);
    mu_assert("Expected TOKEN_EOF after whitespace", token.type == TOKEN_EOF);
    mu_assert("Expected line number to be 2", token.line == 2);

    return NULL;
}

static char* test_scanner_keywords(void) {
    const char* source = "fn let true false null as cond switch raise! try";
    Scanner scanner;
    initScanner(&scanner, source);

    TokenType expected_types[] = {
        TOKEN_FN,   TOKEN_LET,    TOKEN_TRUE,  TOKEN_FALSE, TOKEN_NIL, TOKEN_AS,
        TOKEN_COND, TOKEN_SWITCH, TOKEN_RAISE, TOKEN_TRY,   TOKEN_EOF};

    for (size_t i = 0; i < sizeof(expected_types) / sizeof(expected_types[0]);
         i++) {
        Token token = scanToken(&scanner);
        mu_assert("Unexpected token type", token.type == expected_types[i]);
    }

    return NULL;
}

static char* test_scanner_operators(void) {
    const char* source = "+ - * / % ! != = == < <= > >= && || ^ ~ | & << >>";
    Scanner scanner;
    initScanner(&scanner, source);

    TokenType expected_types[] = {
        TOKEN_PLUS,          TOKEN_MINUS, TOKEN_STAR,       TOKEN_SLASH,
        TOKEN_MODULO,        TOKEN_NOT,   TOKEN_NOT_EQUAL,  TOKEN_EQUAL,
        TOKEN_EQUAL,         TOKEN_LESS,  TOKEN_LESS_EQUAL, TOKEN_GREATER,
        TOKEN_GREATER_EQUAL, TOKEN_AND,   TOKEN_OR,         TOKEN_BXOR,
        TOKEN_BNOT,          TOKEN_BOR,   TOKEN_BAND,       TOKEN_LSHIFT,
        TOKEN_RSHIFT,        TOKEN_EOF};

    for (size_t i = 0; i < sizeof(expected_types) / sizeof(expected_types[0]);
         i++) {
        Token token = scanToken(&scanner);
        mu_assert("Unexpected token type: %s", token.type == expected_types[i]);
    }

    return NULL;
}

// The suite function, called by the main test runner.
void scanner_suite(void) {
    printf("--- Scanner Suite ---\n");
    mu_run_test(test_scanner_whitespace);
    mu_run_test(test_scanner_operators);
    mu_run_test(test_scanner_keywords);
    // TODO: add more tests below
}
