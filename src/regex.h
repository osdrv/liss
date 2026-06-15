#ifndef liss_regex_h
#define liss_regex_h

typedef enum {
    RE_CHAR,
    RE_MATCH,
    RE_JMP,
    RE_SPLIT,
    RE_ANY,
} ReInstrType;

typedef struct {
    ReInstrType type;
    int c;   // char for RE_CHAR
    int s1;  // target 1 for JMP/SPLIT
    int s2;  // target 2 for SPLIT
} ReInstr;

typedef struct {
    ReInstr* instrs;
    int size;
    int start;
} ReProgram;

char* re2postfix(const char* re);
ReProgram* compileRegex(const char* postfix);
bool match(ReProgram* prog, const char* text);

#endif
