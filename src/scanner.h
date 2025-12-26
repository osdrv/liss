#ifndef liss_scanner_h
#define liss_scanner_h

#include "token.h"

typedef struct {
    const char* start;
    const char* current;
    int line;
} Scanner;

void initScanner(Scanner* scanner, const char* source);

Token scanToken(Scanner* scanner);

#endif
