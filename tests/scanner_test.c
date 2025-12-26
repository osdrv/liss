#include "minunit.h"
// TODO: You will need to include your new scanner and token headers here.
#include "scanner.h"
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

static char* test_scanner_operators(void) {
    const char* source = "+ - * / % ! != = == < <= > >= && || ^ ~ | & << >>";
    Scanner scanner;
    initScanner(&scanner, source);

    TokenType expected_types[] = {
        TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_MODULO,
        TOKEN_NOT, TOKEN_NOT_EQUAL, TOKEN_ACCESSOR, TOKEN_EQUAL,
        TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_GREATER, TOKEN_GREATER_EQUAL,
        TOKEN_AND, TOKEN_OR, TOKEN_BXOR, TOKEN_BNOT, TOKEN_BOR, TOKEN_BAND,
        TOKEN_LSHIFT, TOKEN_RSHIFT, TOKEN_EOF
    };

    for (size_t i = 0; i < sizeof(expected_types) / sizeof(expected_types[0]); i++) {
        Token token = scanToken(&scanner);
        mu_assert("Unexpected token type", token.type == expected_types[i]);
    }

    return NULL;
}

// The suite function, called by the main test runner.
void scanner_suite(void) {
    printf("--- Scanner Suite ---\n");
    mu_run_test(test_scanner_whitespace);
    mu_run_test(test_scanner_operators);
    // TODO: add more tests below
}
