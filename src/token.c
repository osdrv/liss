#import "token.h"

const char* printTokenType(TokenType type) {
    switch (type) {
        case TOKEN_FN:
            return "TOKEN_FN";
        case TOKEN_IDENTIFIER:
            return "TOKEN_IDENTIFIER";
        case TOKEN_NUMBER:
            return "TOKEN_NUMBER";
        case TOKEN_STRING:
            return "TOKEN_STRING";
        case TOKEN_TRUE:
            return "TOKEN_TRUE";
        case TOKEN_FALSE:
            return "TOKEN_FALSE";
        case TOKEN_NULL:
            return "TOKEN_NULL";
        case TOKEN_COND:
            return "TOKEN_COND";
        case TOKEN_SWITCH:
            return "TOKEN_SWITCH";
        case TOKEN_LET:
            return "TOKEN_LET";
        case TOKEN_IMPORT:
            return "TOKEN_IMPORT";
        case TOKEN_AS:
            return "TOKEN_AS";
        case TOKEN_BREAKPOINT:
            return "TOKEN_BREAKPOINT";
        case TOKEN_RAISE:
            return "TOKEN_RAISE";
        case TOKEN_TRY:
            return "TOKEN_TRY";
        case TOKEN_AND:
            return "TOKEN_AND";
        case TOKEN_OR:
            return "TOKEN_OR";
        case TOKEN_NOT:
            return "TOKEN_NOT";
        case TOKEN_GREATER:
            return "TOKEN_GREATER";
        case TOKEN_GREATER_EQUAL:
            return "TOKEN_GREATER_EQUAL";
        case TOKEN_LESS:
            return "TOKEN_LESS";
        case TOKEN_LESS_EQUAL:
            return "TOKEN_LESS_EQUAL";
        case TOKEN_BAND:
            return "TOKEN_BAND";
        case TOKEN_BOR:
            return "TOKEN_BOR";
        case TOKEN_BXOR:
            return "TOKEN_BXOR";
        case TOKEN_BNOT:
            return "TOKEN_BNOT";
        case TOKEN_LSHIFT:
            return "TOKEN_LSHIFT";
        case TOKEN_RSHIFT:
            return "TOKEN_RSHIFT";
        case TOKEN_EOF:
            return "TOKEN_EOF";
        default:
            return "UNKNOWN_TOKEN";
    }
}
