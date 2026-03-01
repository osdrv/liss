#ifndef liss_token_h
#define liss_token_h

typedef enum {
    TOKEN_ZERO,  // Sentinel value for uninitialized tokens

    TOKEN_EOF,
    TOKEN_ERROR,

    TOKEN_RAISE_KW,
    TOKEN_TRY_KW,

    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRAKET,
    TOKEN_RBRAKET,

    TOKEN_PLUS_OP,
    TOKEN_PLUS_KW,
    TOKEN_MINUS_OP,
    TOKEN_MINUS_KW,
    TOKEN_STAR_OP,
    TOKEN_STAR_KW,
    TOKEN_SLASH_OP,
    TOKEN_SLASH_KW,
    TOKEN_MODULO_OP,
    TOKEN_MODULO_KW,

    TOKEN_AND_OP,
    TOKEN_AND_KW,
    TOKEN_OR_OP,
    TOKEN_OR_KW,
    TOKEN_NOT_OP,
    TOKEN_NOT_KW,
    TOKEN_EQUAL_OP,
    TOKEN_EQUAL_KW,
    TOKEN_NOT_EQUAL_OP,
    TOKEN_NOT_EQUAL_KW,
    TOKEN_LESS_OP,
    TOKEN_LESS_KW,
    TOKEN_LESS_EQUAL_OP,
    TOKEN_LESS_EQUAL_KW,
    TOKEN_GREATER_OP,
    TOKEN_GREATER_KW,
    TOKEN_GREATER_EQUAL_OP,
    TOKEN_GREATER_EQUAL_KW,

    TOKEN_BAND_OP,
    TOKEN_BAND_KW,
    TOKEN_BOR_OP,
    TOKEN_BOR_KW,
    TOKEN_BXOR_OP,
    TOKEN_BXOR_KW,
    TOKEN_BNOT_OP,
    TOKEN_BNOT_KW,
    TOKEN_LSHIFT_OP,
    TOKEN_LSHIFT_KW,
    TOKEN_RSHIFT_OP,
    TOKEN_RSHIFT_KW,

    TOKEN_FN_KW,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_TRUE_KW,
    TOKEN_FALSE_KW,
    TOKEN_NULL_KW,
    TOKEN_COND_KW,
    TOKEN_SWITCH_KW,
    TOKEN_LET_KW,
    TOKEN_IMPORT_KW,
    TOKEN_AS_KW,
    TOKEN_BREAKPOINT_KW,
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

const char* printTokenType(TokenType type);

#endif
