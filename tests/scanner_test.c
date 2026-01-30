#include "scanner.h"

#include <stdlib.h>
#include <string.h>

#include "common.h"
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
        TOKEN_FN_KW,    TOKEN_LET_KW, TOKEN_TRUE_KW, TOKEN_FALSE_KW,
        TOKEN_NULL_KW,  TOKEN_AS_KW,  TOKEN_COND_KW, TOKEN_SWITCH_KW,
        TOKEN_RAISE_KW, TOKEN_TRY_KW, TOKEN_EOF};

    for (size_t i = 0; i < sizeof(expected_types) / sizeof(expected_types[0]);
         i++) {
        Token token = scanToken(&scanner);
        mu_assert("Unexpected token type", token.type == expected_types[i]);
    }

    return NULL;
}

static char* test_scanner_operators(void) {
    const char* source = "+ - * / % ! != = == < <= > >= & | ^ ~ || && << >>";
    Scanner scanner;
    initScanner(&scanner, source);

    TokenType expected_types[] = {TOKEN_PLUS_OP,
                                  TOKEN_MINUS_OP,
                                  TOKEN_STAR_OP,
                                  TOKEN_SLASH_OP,
                                  TOKEN_MODULO_OP,
                                  TOKEN_NOT_OP,
                                  TOKEN_NOT_EQUAL_OP,
                                  TOKEN_EQUAL_OP,
                                  TOKEN_EQUAL_OP,
                                  TOKEN_LESS_OP,
                                  TOKEN_LESS_EQUAL_OP,
                                  TOKEN_GREATER_OP,
                                  TOKEN_GREATER_EQUAL_OP,
                                  TOKEN_AND_OP,
                                  TOKEN_OR_OP,
                                  TOKEN_BXOR_OP,
                                  TOKEN_BNOT_OP,
                                  TOKEN_BOR_OP,
                                  TOKEN_BAND_OP,
                                  TOKEN_LSHIFT_OP,
                                  TOKEN_RSHIFT_OP,
                                  TOKEN_EOF};

    for (size_t i = 0; i < sizeof(expected_types) / sizeof(expected_types[0]);
         i++) {
        Token token = scanToken(&scanner);
        mu_assert("Unexpected token type: %s", token.type == expected_types[i]);
    }

    return NULL;
}

static char* test_scanner_operator_keywords(void) {
    const char* source = "and or not gt gte lt lte band bor bxor bnot bsl bsr";
    Scanner scanner;
    initScanner(&scanner, source);

    TokenType expected_types[] = {TOKEN_AND_KW,           TOKEN_OR_KW,
                                  TOKEN_NOT_KW,           TOKEN_GREATER_KW,
                                  TOKEN_GREATER_EQUAL_KW, TOKEN_LESS_KW,
                                  TOKEN_LESS_EQUAL_KW,    TOKEN_BAND_KW,
                                  TOKEN_BOR_KW,           TOKEN_BXOR_KW,
                                  TOKEN_BNOT_KW,          TOKEN_LSHIFT_KW,
                                  TOKEN_RSHIFT_KW,        TOKEN_EOF};

    for (size_t i = 0; i < sizeof(expected_types) / sizeof(expected_types[0]);
         i++) {
        Token token = scanToken(&scanner);
        mu_assert("Unexpected token type", token.type == expected_types[i]);
    }

    return NULL;
}

static char* test_scanner_number_literals(void) {
    const char* source = "123 0.123 2.34e05 4.56E-02 0xFFFFFF";
    Scanner scanner;
    initScanner(&scanner, source);

    const char* expected_lexemes[] = {"123", "0.123", "2.34e05", "4.56E-02",
                                      "0xFFFFFF"};

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
    const char* source =
        "\"hello\" \"world\" \"escaped \\\" quote\" \"hello\\nworld\"";
    Scanner scanner;
    initScanner(&scanner, source);

    const char* expected_lexemes[] = {"hello", "world", "escaped \" quote",
                                      "hello\nworld"};

    for (size_t i = 0;
         i < sizeof(expected_lexemes) / sizeof(expected_lexemes[0]); i++) {
        Token token = scanToken(&scanner);
        mu_assert("Expected TOKEN_STRING", token.type == TOKEN_STRING);
        mu_assert("Unexpected lexeme",
                  strcmp(token.start, expected_lexemes[i]) == 0);
        free((void*)token.start);
    }

    return NULL;
}

static char* test_scanner_nested_expression(void) {
    const char* source = "(- (+ 1 2) 3)";
    Scanner scanner;
    initScanner(&scanner, source);

    TokenType expected_types[] = {
        TOKEN_LPAREN, TOKEN_MINUS_OP, TOKEN_LPAREN, TOKEN_PLUS_OP, TOKEN_NUMBER,
        TOKEN_NUMBER, TOKEN_RPAREN,   TOKEN_NUMBER, TOKEN_RPAREN,  TOKEN_EOF};
    for (size_t i = 0; i < sizeof(expected_types) / sizeof(expected_types[0]);
         i++) {
        Token token = scanToken(&scanner);
        mu_assert("Unexpected token type", token.type == expected_types[i]);
    }

    return NULL;
}

static char* test_scanner_unary_minus(void) {
    const char* source = "(-1 -2.5 -0xA)";
    Scanner scanner;
    initScanner(&scanner, source);

    TokenType expected_types[] = {
        TOKEN_LPAREN,   TOKEN_MINUS_OP, TOKEN_NUMBER,
        TOKEN_MINUS_OP, TOKEN_NUMBER,   TOKEN_MINUS_OP,
        TOKEN_NUMBER,   TOKEN_RPAREN,   TOKEN_EOF,
    };
    for (size_t i = 0; i < sizeof(expected_types) / sizeof(expected_types[0]);
         i++) {
        Token token = scanToken(&scanner);
        mu_assert("Unexpected token type", token.type == expected_types[i]);
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
    mu_run_test(test_scanner_nested_expression);
    mu_run_test(test_scanner_unary_minus);
    // TODO: add more tests below
}
