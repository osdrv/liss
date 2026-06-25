#ifndef liss_regex_h
#define liss_regex_h

#define MAX_GROUPS 10

typedef enum {
    RE_CHAR,
    RE_MATCH,
    RE_JMP,
    RE_SPLIT,
    RE_ANY,
    RE_SAVE,
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
    int num_grps;
} ReProgram;

char* re2postfix(const char* re);
ReProgram* compileRegex(const char* postfix);
bool match(ReProgram* prog, const char* text);
bool matchGroups(ReProgram* prog, const char* text,
                 const char* submatch[MAX_GROUPS * 2]);

#endif
