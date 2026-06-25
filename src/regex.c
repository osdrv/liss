#include "regex.h"

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
                if (top >= 0) top--;  // pop '('
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

static void addstate(ThreadList* list, int i, ReProgram* prog, int generation,
                     int* last_visited, const char* submatch[MAX_GROUPS * 2],
                     const char* sp) {
    if (last_visited[i] == generation) return;
    last_visited[i] = generation;

    ReInstr* instr = &prog->instrs[i];

    switch (instr->type) {
        case RE_SPLIT: {
            addstate(list, instr->s1, prog, generation, last_visited, submatch,
                     sp);
            addstate(list, instr->s2, prog, generation, last_visited, submatch,
                     sp);
            break;
        }
        case RE_JMP: {
            addstate(list, instr->s1, prog, generation, last_visited, submatch,
                     sp);
            break;
        }
        case RE_SAVE: {
            const char* new_submatch[MAX_GROUPS * 2];
            memcpy(new_submatch, submatch, sizeof(new_submatch));
            new_submatch[instr->c] = sp;
            addstate(list, instr->s1, prog, generation, last_visited,
                     new_submatch, sp);
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
            case '.': {  // any character
                int i = prog->size++;
                prog->instrs[i] = (ReInstr){RE_ANY, 0, 0, 0};
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
                if (*p >= 1 && *p <= 9) {
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
             init_submatch, sp);

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
            if (instr->type == RE_ANY ||
                (instr->type == RE_CHAR && instr->c == *sp)) {
                addstate(&nlist, instr->s1, prog, generation, last_visited,
                         clist.thread[j].submatch, sp + 1);
            }
        }
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
