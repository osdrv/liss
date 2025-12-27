#ifndef liss_token_h
#define liss_token_h

typedef enum {
    TOKEN_EOF,
    TOKEN_ERROR,

    TOKEN_RAISE,
    TOKEN_TRY,

    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRAKET,
    TOKEN_RBRAKET,
    TOKEN_DOT,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_MODULO,

    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,

    TOKEN_BAND,
    TOKEN_BOR,
    TOKEN_BXOR,
    TOKEN_BNOT,
    TOKEN_LSHIFT,
    TOKEN_RSHIFT,

    TOKEN_FN,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,
    TOKEN_COND,
    TOKEN_SWITCH,
    TOKEN_LET,
    TOKEN_IMPORT,
    TOKEN_AS,
    TOKEN_BREAKPOINT,
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

const char* printTokenType(TokenType type);

#endif
