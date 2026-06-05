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
    int* instr_ixs;
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
                     int* last_visited) {
    // we've already added this instruction in this step so skip it
    if (last_visited[i] == generation) return;
    last_visited[i] = generation;

    ReInstr* instr = &prog->instrs[i];

    switch (instr->type) {
        case RE_SPLIT: {
            addstate(list, instr->s1, prog, generation, last_visited);
            addstate(list, instr->s2, prog, generation, last_visited);
            break;
        }
        case RE_JMP: {
            addstate(list, instr->s1, prog, generation, last_visited);
            break;
        }
        default: {
            // A char or a match, stop here and wait for the next input char
            list->instr_ixs[list->size++] = i;
            break;
        }
    }
}

ReProgram* compileRegex(const char* postfix) {
    int len = strlen(postfix);
    ReProgram* prog = malloc(sizeof(ReProgram));
    prog->instrs = malloc(sizeof(ReInstr) * (len + 1));
    prog->size = 0;

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
                int i = prog->size++;
                prog->instrs[i] = (ReInstr){RE_CHAR, *p, 0, 0};
                stack[++top] = (Frag){i, list1(&prog->instrs[i].s1)};
                break;
            }
        }
    }

    Frag final = stack[top--];
    int match_idx = prog->size++;
    prog->instrs[match_idx] = (ReInstr){RE_MATCH, 0, 0, 0};
    patch(final.out, match_idx);

    prog->start = final.start;
    return prog;
}

bool match(ReProgram* prog, const char* text) {
    int n_instr = prog->size;
    int* last_visited = calloc(n_instr, sizeof(int));
    int generation = 1;

    // 2 lists: current and next
    ThreadList clist = {malloc(sizeof(int) * n_instr), 0};
    ThreadList nlist = {malloc(sizeof(int) * n_instr), 0};

    addstate(&clist, prog->start, prog, generation++, last_visited);

    for (; *text; text++) {
        nlist.size = 0;  // clear the next list
        for (int j = 0; j < clist.size; j++) {
            ReInstr* instr = &prog->instrs[clist.instr_ixs[j]];
            if (instr->type == RE_ANY ||
                (instr->type == RE_CHAR && instr->c == *text)) {
                addstate(&nlist, instr->s1, prog, generation, last_visited);
            }
        }
        // swap the lists
        ThreadList tmp = clist;
        clist = nlist;
        nlist = tmp;
        generation++;

        if (clist.size == 0) break;  // no more options to explore
    }

    bool found = false;
    for (int j = 0; j < clist.size; j++) {
        if (prog->instrs[clist.instr_ixs[j]].type == RE_MATCH) {
            found = true;
            break;
        }
    }

    free(clist.instr_ixs);
    free(nlist.instr_ixs);
    free(last_visited);
    return found;
}
