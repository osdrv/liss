#include "repl.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "common.h"
#include "value.h"
#include "vm.h"

#define HISTORY_MAX 100

typedef struct {
    char buf[LINE_MAX];
    int len;
    int cur;
} Line;

typedef struct {
    char* entries[HISTORY_MAX];
    int cnt;
    int ix;
} History;

static const char* PROMPT = "> ";
static const int PROMPT_LEN = 2;

static const char* LISS_REPL_HELLO = "LISS REPL. Type Ctrl+D (EOF) to exit.\n";

static struct termios orig_termios;

static void disableRawMode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enableRawMode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(ICRNL | IXON);  // disable CR->LF translation
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void lineRefresh(Line* l) {
    write(STDOUT_FILENO, "\r", 1);
    write(STDOUT_FILENO, PROMPT, PROMPT_LEN);
    write(STDOUT_FILENO, l->buf, l->len);
    write(STDOUT_FILENO, "\x1b[0K", 4);
    write(STDOUT_FILENO, "\r", 1);
    char seq[16];
    int n = snprintf(seq, sizeof(seq), "\x1b[%dC", PROMPT_LEN + l->cur);
    write(STDOUT_FILENO, seq, n);
}

static void historyAdd(History* hist, const char* line) {
    if (line[0] == '\0') return;
    if (hist->cnt > 0 && strcmp(hist->entries[hist->cnt - 1], line) == 0)
        return;

    if (hist->cnt == HISTORY_MAX) {
        free(hist->entries[0]);
        memmove(&hist->entries[0], &hist->entries[1],
                (HISTORY_MAX - 1) * sizeof(char*));
        hist->cnt--;
    }
    hist->entries[hist->cnt++] = strdup(line);
}

static char* lineRead(History* hist) {
    Line l = {.len = 0, .cur = 0};

    char saved[LINE_MAX] = {0};
    bool navigating = false;

    write(STDOUT_FILENO, PROMPT, PROMPT_LEN);

    for (;;) {
        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) return NULL;  // EOF || Ctrl+D

        if (c == '\r' || c == '\n') {
            write(STDOUT_FILENO, "\n", 1);
            l.buf[l.len] = '\0';
            navigating = false;
            return strdup(l.buf);
        } else if (c == '\x7f' || c == '\b') {  // backspace
            if (l.cur > 0) {
                memmove(&l.buf[l.cur - 1], &l.buf[l.cur], l.len - l.cur);
                l.cur--;
                l.len--;
                lineRefresh(&l);
            }
        } else if (c == '\x1b') {
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;
            if (seq[0] == '[') {
                if (seq[1] == 'C' && l.cur < l.len) {
                    l.cur++;
                    lineRefresh(&l);
                } else if (seq[1] == 'D' && l.cur > 0) {
                    l.cur--;
                    lineRefresh(&l);
                } else if (seq[1] == 'A') {  // ↑ — go back in history
                    if (hist->cnt == 0) continue;
                    if (!navigating) {
                        memcpy(saved, l.buf, l.len + 1);
                        navigating = true;
                        hist->ix = hist->cnt;
                    }
                    if (hist->ix > 0) {
                        hist->ix--;
                        int elen = strlen(hist->entries[hist->ix]);
                        memcpy(l.buf, hist->entries[hist->ix], elen + 1);
                        l.len = l.cur = elen;
                        lineRefresh(&l);
                    }
                } else if (seq[1] == 'B') {  // ↓ — go forward
                    if (!navigating) continue;
                    hist->ix++;
                    if (hist->ix >= hist->cnt) {
                        int slen = strlen(saved);
                        memcpy(l.buf, saved, slen + 1);
                        l.len = l.cur = slen;
                        navigating = false;
                    } else {
                        int elen = strlen(hist->entries[hist->ix]);
                        memcpy(l.buf, hist->entries[hist->ix], elen + 1);
                        l.len = l.cur = elen;
                    }
                    lineRefresh(&l);
                }
            }
        } else if (c == '\x03') {
            write(STDOUT_FILENO, "\n", 1);
            return NULL;  // EOF/exit
        } else if (c >= 32 && l.len < LINE_MAX - 1) {
            memmove(&l.buf[l.cur + 1], &l.buf[l.cur], l.len - l.cur);
            l.buf[l.cur] = c;
            l.cur++;
            l.len++;
            lineRefresh(&l);
        }
    }
}

void runRepl(VMOptions options) {
    VM* vm = newVM(options);

    enableRawMode();

    write(STDOUT_FILENO, LISS_REPL_HELLO, strlen(LISS_REPL_HELLO));

    History* hist = calloc(1, sizeof(History));

    for (;;) {
        char* line = lineRead(hist);
        if (line == NULL) break;

        historyAdd(hist, line);

        InterpretResult result = interpret(vm, line, NULL);
        if (result == INTERPRET_COMPILE_ERROR) {
            ERROR_LOG("%s", vm->error_msg);
        } else if (result == INTERPRET_RUNTIME_ERROR) {
            char* str = sprintValue(vm->raise_value);
            ERROR_LOG("%s", str);
            free(str);
        } else if (result == INTERPRET_OK) {
            // Print the last popped value
            char* str = sprintValue(vm->last_popped_value);
            PRINTF("%s\n", str);
            fflush(stdout);
            free(str);
        }
    }

    for (int i = 0; i < hist->cnt; i++) free(hist->entries[i]);
    free(hist);
    destroyVM(vm);
}
