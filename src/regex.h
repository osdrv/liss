#ifndef liss_regex_h
#define liss_regex_h

#include <stdint.h>

#define MAX_GROUPS   10
#define MAX_CHARSETS 32

typedef enum {
    RE_CHAR,
    RE_MATCH,
    RE_JMP,
    RE_SPLIT,
    RE_ANY,
    RE_SAVE,
    RE_CLASS,  // c encodes class: 'd' digit, 'w' word, 'W' non-word
    RE_BOL,     // ^ — zero-width: succeeds at start of string
    RE_EOL,     // $ — zero-width: succeeds at end of string
    RE_BRACKET, // [...] — c is index into prog->charsets
} ReInstrType;

typedef struct {
    uint8_t bits[32];  // 256-bit set: bit i set iff char i matches
} ReCharset;

// Sentinel bytes used in the postfix string to represent escape classes.
// Must be outside 1-9 (group IDs) and not regex operator chars.
#define RE_ESC_DIGIT    11
#define RE_ESC_WORD     12
#define RE_ESC_NONWORD  14
#define RE_ESC_SPACE    15
#define RE_ESC_NONSPACE 16
#define RE_ESC_TAB      17
#define RE_ESC_NEWLINE  18

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
    ReCharset charsets[MAX_CHARSETS];
    int num_charsets;
} ReProgram;

char* re2postfix(const char* re);
ReProgram* compileRegex(const char* postfix);
ReProgram* compilePattern(const char* re);  // handles [...] in addition to the above
bool match(ReProgram* prog, const char* text);
bool matchGroups(ReProgram* prog, const char* text,
                 const char* submatch[MAX_GROUPS * 2]);

#endif
