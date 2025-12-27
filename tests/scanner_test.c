#include "scanner.h"

#include <string.h>

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

    TokenType expected_types[] = {TOKEN_FN,    TOKEN_LET,    TOKEN_TRUE,
                                  TOKEN_FALSE, TOKEN_NULL,   TOKEN_AS,
                                  TOKEN_COND,  TOKEN_SWITCH, TOKEN_RAISE,
                                  TOKEN_TRY,   TOKEN_EOF};

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

static char* test_scanner_operator_keywords(void) {
    const char* source =
        "and or not gt gte lt lte band bor bxor bnot bsl bsr";
    Scanner scanner;
    initScanner(&scanner, source);

    TokenType expected_types[] = {
        TOKEN_AND,           TOKEN_OR,   TOKEN_NOT,        TOKEN_GREATER,
        TOKEN_GREATER_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_BAND,
        TOKEN_BOR,           TOKEN_BXOR, TOKEN_BNOT,       TOKEN_LSHIFT,
        TOKEN_RSHIFT,        TOKEN_EOF};

    for (size_t i = 0; i < sizeof(expected_types) / sizeof(expected_types[0]);
         i++) {
        Token token = scanToken(&scanner);
        mu_assert("Unexpected token type", token.type == expected_types[i]);
    }

    return NULL;
}

static char* test_scanner_number_literals(void) {
    const char* source = "123 -456 +789 0.123 2.34e05 4.56E-02 0xFFFFFF";
    Scanner scanner;
    initScanner(&scanner, source);

    const char* expected_lexemes[] = {"123",     "-456",     "+789",    "0.123",
                                      "2.34e05", "4.56E-02", "0xFFFFFF"};

    for (size_t i = 0;
         i < sizeof(expected_lexemes) / sizeof(expected_lexemes[0]); i++) {
        Token token = scanToken(&scanner);
        mu_assert("Expected TOKEN_NUMBER", token.type == TOKEN_NUMBER);
        mu_assert("Unexpected lexeme",
                  strncmp(token.start, expected_lexemes[i], token.length) == 0);
    }

    return NULL;
}

static char* test_scanner_string_literals(void) {
    const char* source = "\"hello\" \"world\" \"escaped \\\" quote\"";
    Scanner scanner;
    initScanner(&scanner, source);

    const char* expected_lexemes[] = {"\"hello\"", "\"world\"",
                                      "\"escaped \\\" quote\""};

    for (size_t i = 0;
         i < sizeof(expected_lexemes) / sizeof(expected_lexemes[0]); i++) {
        Token token = scanToken(&scanner);
        mu_assert("Expected TOKEN_STRING", token.type == TOKEN_STRING);
        mu_assert("Unexpected lexeme",
                  strncmp(token.start, expected_lexemes[i], token.length) == 0);
    }

    return NULL;
}

void scanner_suite(void) {
    printf("--- Scanner Suite ---\n");
    mu_run_test(test_scanner_whitespace);
    mu_run_test(test_scanner_operators);
    mu_run_test(test_scanner_keywords);
    mu_run_test(test_scanner_operator_keywords);
    mu_run_test(test_scanner_number_literals);
    mu_run_test(test_scanner_string_literals);
    // TODO: add more tests below
}
