#include "regex.h"

#include <stdlib.h>
#include <string.h>

static int getPrecedence(char op) {
    switch (op) {
        case '*':
        case '+':
        case '?':
            return 3;
        case '.':
            return 2;
        case '|':
            return 1;
        default:
            return 0;
    }
}

char* addConcat(const char* re) {
    int len = strlen(re);
    char* res = malloc(len * 2 + 1);
    int j = 0;

    for (int i = 0; i < len; i++) {
        char c1 = re[i];
        res[j++] = c1;
        if (c1 == '(' || c1 == '|') continue;
        if (i + 1 < len) {
            char c2 = re[i + 1];
            switch (c2) {
                case ')':
                case '|':
                case '*':
                case '+':
                case '?':
                    break;
                default:
                    // if the next char is a literal or an open paren, we join
                    res[j++] = '.';
                    break;
            }
        }
    }
    res[j] = '\0';
    return res;
}

char* infixToPostfix(const char* infix) {
    int len = strlen(infix);
    char* postfix = malloc(len + 1);
    char stack[1024];
    int top = -1;
    int j = 0;

    for (int i = 0; i < len; i++) {
        char c = infix[i];

        switch (c) {
            case '(':
                stack[++top] = c;
                break;
            case ')':
                while (top >= 0 && stack[top] != '(') {
                    postfix[j++] = stack[top--];
                }
                top--;
                break;
            case '|':
            case '*':
            case '+':
            case '?':
            case '.':
                while (top >= 0 && stack[top] != '(' &&
                       getPrecedence(stack[top]) >= getPrecedence(c)) {
                    postfix[j++] = stack[top--];
                }
                stack[++top] = c;
                break;
            default:
                postfix[j++] = c;
        }
    }

    while (top >= 0) {
        postfix[j++] = stack[top--];
    }
    postfix[j] = '\0';
    return postfix;
}

char* re2postfix(const char* re) {
    char* dotted = addConcat(re);
    char* postfix = infixToPostfix(dotted);
    free(dotted);

    return postfix;
}
