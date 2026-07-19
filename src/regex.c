#include "regex.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct PtrList PtrList;
struct PtrList {
    int* field;
    PtrList* next;
};

typedef struct {
    int start;
    PtrList* out;
} Frag;

typedef struct {
    int instr_ix;
    const char* submatch[MAX_GROUPS * 2];
} Thread;

typedef struct {
    Thread* thread;
    int size;
} ThreadList;

static PtrList* list1(int* field) {
    PtrList* list = malloc(sizeof(PtrList));
    list->field = field;
    list->next = NULL;
    return list;
}

static void patch(PtrList* list, int target) {
    PtrList* next;
    for (; list; list = next) {
        next = list->next;
        *list->field = target;
        free(list);
    }
}

static PtrList* append(PtrList* list1, PtrList* list2) {
    if (!list1) return list2;
    PtrList* tmp = list1;
    while (tmp->next) tmp = tmp->next;
    tmp->next = list2;
    return list1;
}

static int getPrecedence(char op) {
    switch (op) {
        case '*':
        case '+':
        case '?':
            return 3;
        case '@':
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
        char emit;

        if (c1 == '\\' && i + 1 < len) {
            switch (re[i + 1]) {
                case 'd':
                    emit = RE_ESC_DIGIT;
                    i++;
                    break;
                case 'w':
                    emit = RE_ESC_WORD;
                    i++;
                    break;
                case 'W':
                    emit = RE_ESC_NONWORD;
                    i++;
                    break;
                case 's':
                    emit = RE_ESC_SPACE;
                    i++;
                    break;
                case 'S':
                    emit = RE_ESC_NONSPACE;
                    i++;
                    break;
                case 't':
                    emit = RE_ESC_TAB;
                    i++;
                    break;
                case 'n':
                    emit = RE_ESC_NEWLINE;
                    i++;
                    break;
                default:
                    emit = c1;
                    break;
            }
        } else {
            emit = c1;
        }

        res[j++] = emit;
        if (emit == '(' || emit == '|') continue;
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
                    res[j++] = '@';
                    break;
            }
        }
    }
    res[j] = '\0';
    return res;
}

char* infixToPostfix(const char* infix) {
    int len = strlen(infix);
    char* postfix = malloc(len * 2 + 1);
    char stack[1024];
    int top = -1;
    int j = 0;

    int group_stack[100];
    int group_stack_top = -1;
    int next_group_id = 1;

    for (int i = 0; i < len; i++) {
        char c = infix[i];
        switch (c) {
            case '(':
                if (next_group_id < MAX_GROUPS) {
                    group_stack[++group_stack_top] = next_group_id++;
                } else {
                    group_stack[++group_stack_top] = 0;
                }
                stack[++top] = c;
                break;
            case ')':
                while (top >= 0 && stack[top] != '(') {
                    postfix[j++] = stack[top--];
                }
                if (top < 0) {  // unmatched ')'
                    free(postfix);
                    return NULL;
                }
                top--;  // pop '('
                if (group_stack_top >= 0) {
                    int g = group_stack[group_stack_top--];
                    if (g > 0) {
                        postfix[j++] = (char)g;
                    }
                }
                break;
            case '|':
            case '*':
            case '+':
            case '?':
            case '@':
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
        if (stack[top] == '(') {  // unmatched '('
            free(postfix);
            return NULL;
        }
        postfix[j++] = stack[top--];
    }
    postfix[j] = '\0';
    return postfix;
}

// Replaces each [...] in re with a sentinel byte (128 + charset_index),
// parsing the charset bitmap into prog->charsets.  Returns a malloc'd string
// the caller must free; returns NULL on parse error.
static char* replaceBrackets(const char* re, ReProgram* prog) {
    int len = strlen(re);
    char* out = malloc(len + 1);
    int j = 0;

    for (int i = 0; i < len; i++) {
        if (re[i] != '[') {
            out[j++] = re[i];
            continue;
        }
        // find matching ']'
        int end = i + 1;
        while (end < len && re[end] != ']') end++;
        if (end >= len) {
            free(out);
            return NULL;
        }  // unterminated

        if (prog->num_charsets >= MAX_CHARSETS) {
            free(out);
            return NULL;
        }

        ReCharset* cs = &prog->charsets[prog->num_charsets];
        memset(cs->bits, 0, sizeof(cs->bits));

        int k = i + 1;
        bool negate = (k < end && re[k] == '^');
        if (negate) k++;

        // ']' as first char after '[' or '[^' is treated as literal
        if (k < end && re[k] == ']') {
            cs->bits[(unsigned char)']' / 8] |= 1u << (']' % 8);
            k++;
        }

        while (k < end) {
            unsigned char lo = (unsigned char)re[k];
            if (k + 2 < end && re[k + 1] == '-') {
                unsigned char hi = (unsigned char)re[k + 2];
                for (int c = lo; c <= hi; c++) cs->bits[c / 8] |= 1u << (c % 8);
                k += 3;
            } else {
                cs->bits[lo / 8] |= 1u << (lo % 8);
                k++;
            }
        }

        if (negate) {
            for (int b = 0; b < 32; b++) cs->bits[b] ^= 0xFF;
            cs->bits[0] &= ~1u;  // never match '\0'
        }

        out[j++] = (char)(128 + prog->num_charsets);
        prog->num_charsets++;
        i = end;  // skip past ']'
    }
    out[j] = '\0';
    return out;
}

char* re2postfix(const char* re) {
    char* dotted = addConcat(re);
    char* postfix = infixToPostfix(dotted);
    free(dotted);
    return postfix;
}

// All-in-one: handles [...], escape classes, anchors, groups.
ReProgram* compilePattern(const char* re) {
    // Phase 1: parse [...] into charsets, replace with sentinel bytes.
    // We need a temp ReProgram shell just to collect charsets.
    ReProgram tmp;
    memset(&tmp, 0, sizeof(tmp));

    char* expanded = replaceBrackets(re, &tmp);
    if (expanded == NULL) return NULL;

    // Phase 2: normal pipeline on the expanded string.
    char* dotted = addConcat(expanded);
    free(expanded);
    char* postfix = infixToPostfix(dotted);
    free(dotted);
    if (postfix == NULL) return NULL;

    // Phase 3: compile — compileRegex emits RE_BRACKET for sentinel bytes.
    ReProgram* prog = compileRegex(postfix);
    free(postfix);
    if (prog == NULL) return NULL;

    // Splice our charsets into the compiled program.
    prog->num_charsets = tmp.num_charsets;
    memcpy(prog->charsets, tmp.charsets, sizeof(ReCharset) * tmp.num_charsets);
    return prog;
}

static void addstate(ThreadList* list, int i, ReProgram* prog, int generation,
                     int* last_visited, const char* submatch[MAX_GROUPS * 2],
                     const char* sp, const char* text_start) {
    if (last_visited[i] == generation) return;
    last_visited[i] = generation;

    ReInstr* instr = &prog->instrs[i];

    switch (instr->type) {
        case RE_SPLIT: {
            addstate(list, instr->s1, prog, generation, last_visited, submatch,
                     sp, text_start);
            addstate(list, instr->s2, prog, generation, last_visited, submatch,
                     sp, text_start);
            break;
        }
        case RE_JMP: {
            addstate(list, instr->s1, prog, generation, last_visited, submatch,
                     sp, text_start);
            break;
        }
        case RE_SAVE: {
            const char* new_submatch[MAX_GROUPS * 2];
            memcpy(new_submatch, submatch, sizeof(new_submatch));
            new_submatch[instr->c] = sp;
            addstate(list, instr->s1, prog, generation, last_visited,
                     new_submatch, sp, text_start);
            break;
        }
        case RE_BOL: {
            if (sp == text_start)
                addstate(list, instr->s1, prog, generation, last_visited,
                         submatch, sp, text_start);
            break;
        }
        case RE_EOL: {
            if (*sp == '\0')
                addstate(list, instr->s1, prog, generation, last_visited,
                         submatch, sp, text_start);
            break;
        }
        default: {
            Thread* t = &list->thread[list->size++];
            t->instr_ix = i;
            memcpy(t->submatch, submatch, sizeof(t->submatch));
            break;
        }
    }
}

ReProgram* compileRegex(const char* postfix) {
    int len = strlen(postfix);
    ReProgram* prog = malloc(sizeof(ReProgram));
    prog->instrs = malloc(sizeof(ReInstr) * (len * 2 + 10));
    prog->size = 0;
    prog->num_charsets = 0;

    int max_grp = 0;
    for (const char* p = postfix; *p; p++) {
        if (*p >= 1 && *p <= 9) {
            if (*p > max_grp) max_grp = *p;
        }
    }
    prog->num_grps = max_grp + 1;  // the complete match is group 0

    Frag stack[1024];
    int top = -1;

    for (const char* p = postfix; *p; p++) {
        switch (*p) {
            case '@': {
                Frag e2 = stack[top--];
                Frag e1 = stack[top--];
                patch(e1.out, e2.start);
                stack[++top] = (Frag){e1.start, e2.out};
                break;
            }
            case '.': {
                int i = prog->size++;
                prog->instrs[i] = (ReInstr){RE_ANY, 0, 0, 0};
                stack[++top] = (Frag){i, list1(&prog->instrs[i].s1)};
                break;
            }
            case '^': {
                int i = prog->size++;
                prog->instrs[i] = (ReInstr){RE_BOL, 0, 0, 0};
                stack[++top] = (Frag){i, list1(&prog->instrs[i].s1)};
                break;
            }
            case '$': {
                int i = prog->size++;
                prog->instrs[i] = (ReInstr){RE_EOL, 0, 0, 0};
                stack[++top] = (Frag){i, list1(&prog->instrs[i].s1)};
                break;
            }
            case RE_ESC_DIGIT:
            case RE_ESC_WORD:
            case RE_ESC_NONWORD:
            case RE_ESC_SPACE:
            case RE_ESC_NONSPACE: {
                int cls = (*p == RE_ESC_DIGIT)     ? 'd'
                          : (*p == RE_ESC_WORD)    ? 'w'
                          : (*p == RE_ESC_NONWORD) ? 'W'
                          : (*p == RE_ESC_SPACE)   ? 's'
                                                   : 'S';
                int i = prog->size++;
                prog->instrs[i] = (ReInstr){RE_CLASS, cls, 0, 0};
                stack[++top] = (Frag){i, list1(&prog->instrs[i].s1)};
                break;
            }
            case RE_ESC_TAB:
            case RE_ESC_NEWLINE: {
                int ch = (*p == RE_ESC_TAB) ? '\t' : '\n';
                int i = prog->size++;
                prog->instrs[i] = (ReInstr){RE_CHAR, ch, 0, 0};
                stack[++top] = (Frag){i, list1(&prog->instrs[i].s1)};
                break;
            }
            case '|': {  // alternate
                Frag e2 = stack[top--];
                Frag e1 = stack[top--];
                int i = prog->size++;
                prog->instrs[i] = (ReInstr){RE_SPLIT, 0, e1.start, e2.start};
                stack[++top] = (Frag){i, append(e1.out, e2.out)};
                break;
            }
            case '*': {
                Frag e = stack[top--];
                int i = prog->size++;
                prog->instrs[i] = (ReInstr){RE_SPLIT, 0, e.start, 0};
                patch(e.out, i);
                stack[++top] = (Frag){i, list1(&prog->instrs[i].s2)};
                break;
            }
            case '+': {
                Frag e = stack[top--];
                int i = prog->size++;
                prog->instrs[i] = (ReInstr){RE_SPLIT, 0, e.start, 0};
                patch(e.out, i);
                stack[++top] = (Frag){e.start, list1(&prog->instrs[i].s2)};
                break;
            }
            case '?': {
                Frag e = stack[top--];
                int i = prog->size++;
                prog->instrs[i] = (ReInstr){RE_SPLIT, 0, e.start, 0};
                stack[++top] =
                    (Frag){i, append(e.out, list1(&prog->instrs[i].s2))};
                break;
            }
            default: {
                unsigned char uc = (unsigned char)*p;
                if (uc >= 128) {
                    // bracket class sentinel: index = uc - 128
                    int i = prog->size++;
                    prog->instrs[i] = (ReInstr){RE_BRACKET, uc - 128, 0, 0};
                    stack[++top] = (Frag){i, list1(&prog->instrs[i].s1)};
                } else if (*p >= 1 && *p <= 9) {
                    int g = *p;
                    Frag e = stack[top--];
                    int s_start = prog->size++;
                    int s_end = prog->size++;
                    prog->instrs[s_start] =
                        (ReInstr){RE_SAVE, 2 * g, e.start, 0};
                    prog->instrs[s_end] = (ReInstr){RE_SAVE, 2 * g + 1, 0, 0};
                    patch(e.out, s_end);
                    stack[++top] =
                        (Frag){s_start, list1(&prog->instrs[s_end].s1)};
                } else {
                    int i = prog->size++;
                    prog->instrs[i] = (ReInstr){RE_CHAR, *p, 0, 0};
                    stack[++top] = (Frag){i, list1(&prog->instrs[i].s1)};
                }
                break;
            }
        }
    }

    Frag final = stack[top--];
    int start_save = prog->size++;
    int end_save = prog->size++;
    int match_idx = prog->size++;

    prog->instrs[start_save] = (ReInstr){RE_SAVE, 0, final.start, 0};
    prog->instrs[end_save] = (ReInstr){RE_SAVE, 1, match_idx, 0};
    prog->instrs[match_idx] = (ReInstr){RE_MATCH, 0, 0, 0};

    patch(final.out, end_save);
    prog->start = start_save;
    return prog;
}

bool matchGroups(ReProgram* prog, const char* text,
                 const char* submatch[MAX_GROUPS * 2]) {
    int n_instr = prog->size;
    int* last_visited = calloc(n_instr, sizeof(int));
    int generation = 1;

    const char* init_submatch[MAX_GROUPS * 2];
    memset(init_submatch, 0, sizeof(init_submatch));

    ThreadList clist = {malloc(sizeof(Thread) * n_instr), 0};
    ThreadList nlist = {malloc(sizeof(Thread) * n_instr), 0};

    const char* sp = text;
    addstate(&clist, prog->start, prog, generation++, last_visited,
             init_submatch, sp, text);

    const char* matched_submatch[MAX_GROUPS * 2];
    bool matched = false;

    for (;;) {
        for (int j = 0; j < clist.size; j++) {
            if (prog->instrs[clist.thread[j].instr_ix].type == RE_MATCH) {
                memcpy(matched_submatch, clist.thread[j].submatch,
                       sizeof(matched_submatch));
                matched = true;
                break;
            }
        }

        if (*sp == '\0') break;

        nlist.size = 0;
        for (int j = 0; j < clist.size; j++) {
            ReInstr* instr = &prog->instrs[clist.thread[j].instr_ix];
            unsigned char ch = (unsigned char)*sp;
            bool is_word = isalnum(ch) || *sp == '_';
            bool advance =
                instr->type == RE_ANY ||
                (instr->type == RE_CHAR && instr->c == *sp) ||
                (instr->type == RE_CLASS && instr->c == 'd' && isdigit(ch)) ||
                (instr->type == RE_CLASS && instr->c == 'w' && is_word) ||
                (instr->type == RE_CLASS && instr->c == 'W' && !is_word) ||
                (instr->type == RE_CLASS && instr->c == 's' && isspace(ch)) ||
                (instr->type == RE_CLASS && instr->c == 'S' && !isspace(ch)) ||
                (instr->type == RE_BRACKET &&
                 (prog->charsets[instr->c].bits[ch / 8] >> (ch % 8) & 1));
            if (advance) {
                addstate(&nlist, instr->s1, prog, generation, last_visited,
                         clist.thread[j].submatch, sp + 1, text);
            }
        }
        // Always try starting a fresh match at the next position so the
        // engine finds matches that don't begin at offset 0. Existing
        // threads are advanced first (above), so leftmost wins when two
        // threads compete for the same NFA state in this generation.
        addstate(&nlist, prog->start, prog, generation, last_visited,
                 init_submatch, sp + 1, text);

        ThreadList tmp = clist;
        clist = nlist;
        nlist = tmp;
        generation++;
        sp++;

        if (clist.size == 0) break;
    }

    if (matched) {
        memcpy(submatch, matched_submatch, sizeof(matched_submatch));
    }

    free(clist.thread);
    free(nlist.thread);
    free(last_visited);

    return matched;
}

bool match(ReProgram* prog, const char* text) {
    const char* submatch[MAX_GROUPS * 2];
    return matchGroups(prog, text, submatch);
}
