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
        case TOKEN_NIL:
            return "TOKEN_NIL";
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
        default:
            return "UNKNOWN_TOKEN";
    }
}
