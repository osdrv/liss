#include "str.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "value.h"
#include "vm.h"

typedef struct {
    const char* start;
    int len;
} Seg;

static Value upperNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_STRING(argv[0])) {
        RUNTIME_ERR(vm, "upper expects a string");
        return NIL_VAL;
    }
    ObjString* s = AS_STRING(argv[0]);
    char* buf = malloc(s->length + 1);
    if (buf == NULL) {
        RUNTIME_ERR(vm, "out of memory");
        return NIL_VAL;
    }
    for (int i = 0; i < s->length; i++)
        buf[i] = toupper((unsigned char)s->chars[i]);
    buf[s->length] = '\0';
    return OBJ_VAL(takeString(vm, buf, s->length));
}

static Value lowerNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_STRING(argv[0])) {
        RUNTIME_ERR(vm, "lower expects a string");
        return NIL_VAL;
    }
    ObjString* s = AS_STRING(argv[0]);
    char* buf = malloc(s->length + 1);
    if (buf == NULL) {
        RUNTIME_ERR(vm, "out of memory");
        return NIL_VAL;
    }
    for (int i = 0; i < s->length; i++)
        buf[i] = tolower((unsigned char)s->chars[i]);
    buf[s->length] = '\0';
    return OBJ_VAL(takeString(vm, buf, s->length));
}

static Value trimNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_STRING(argv[0])) {
        RUNTIME_ERR(vm, "trim expects a string");
        return NIL_VAL;
    }
    ObjString* s = AS_STRING(argv[0]);
    int start = 0;
    int end = s->length - 1;
    while (start <= end && isspace((unsigned char)s->chars[start])) start++;
    while (end >= start && isspace((unsigned char)s->chars[end])) end--;
    return OBJ_VAL(copyString(vm, s->chars + start, end - start + 1));
}

static Value containsNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_STRING(argv[0]) || !IS_STRING(argv[1])) {
        RUNTIME_ERR(vm, "contains? expects exactly 2 strings");
        return NIL_VAL;
    }
    ObjString* haystack = AS_STRING(argv[0]);
    ObjString* needle = AS_STRING(argv[1]);
    return BOOL_VAL(strstr(haystack->chars, needle->chars) != NULL);
}

static Value startsWithNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_STRING(argv[0]) || !IS_STRING(argv[1])) {
        RUNTIME_ERR(vm, "starts_with? expects exactly 2 strings");
        return NIL_VAL;
    }
    ObjString* haystack = AS_STRING(argv[0]);
    ObjString* needle = AS_STRING(argv[1]);
    return BOOL_VAL(strncmp(haystack->chars, needle->chars, needle->length) ==
                    0);
}

static Value endsWithNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_STRING(argv[0]) || !IS_STRING(argv[1])) {
        RUNTIME_ERR(vm, "starts_with? expects exactly 2 strings");
        return NIL_VAL;
    }
    ObjString* haystack = AS_STRING(argv[0]);
    ObjString* needle = AS_STRING(argv[1]);

    if (needle->length > haystack->length) return BOOL_VAL(false);
    int offset = haystack->length - needle->length;
    return BOOL_VAL(
        strncmp(haystack->chars + offset, needle->chars, needle->length) == 0);
}

static Value indexOfNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_STRING(argv[0]) || !IS_STRING(argv[1])) {
        RUNTIME_ERR(vm, "index_of expects exactly 2 strings");
        return NIL_VAL;
    }
    ObjString* haystack = AS_STRING(argv[0]);
    ObjString* needle = AS_STRING(argv[1]);
    char* found = strstr(haystack->chars, needle->chars);
    if (found == NULL) return INT_VAL(-1);
    return INT_VAL((int64_t)(found - haystack->chars));
}

static Value substrNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_STRING(argv[0]) || !IS_INT(argv[1]) || !IS_INT(argv[2])) {
        RUNTIME_ERR(vm, "substr expects string, int, int");
        return NIL_VAL;
    }
    ObjString* s = AS_STRING(argv[0]);
    int64_t start = AS_INT(argv[1]);
    int64_t len = AS_INT(argv[2]);
    if (start < 0 || start > s->length) {
        return OBJ_VAL(newError(vm, "substr: start out of bounds"));
    }
    if (len < 0) {
        return OBJ_VAL(newError(vm, "substr: length must be non-negative"));
    }
    if (start + len > s->length) len = s->length - start;
    return OBJ_VAL(copyString(vm, s->chars + (int)start, (int)len));
}

static Value replaceNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_STRING(argv[0]) || !IS_STRING(argv[1]) || !IS_STRING(argv[2])) {
        RUNTIME_ERR(vm, "replace expects three strings");
        return NIL_VAL;
    }
    ObjString* s = AS_STRING(argv[0]);
    ObjString* from = AS_STRING(argv[1]);
    ObjString* to = AS_STRING(argv[2]);

    char* pos = strstr(s->chars, from->chars);
    if (pos == NULL) return argv[0];  // not found — return original unchanged

    int prefix = (int)(pos - s->chars);
    int result_len = s->length - from->length + to->length;
    char* buf = malloc(result_len + 1);
    if (buf == NULL) {
        RUNTIME_ERR(vm, "out of memory");
        return NIL_VAL;
    }

    memcpy(buf, s->chars, prefix);
    memcpy(buf + prefix, to->chars, to->length);
    memcpy(buf + prefix + to->length, pos + from->length,
           s->length - prefix - from->length + 1);  // +1 copies the '\0'
    return OBJ_VAL(takeString(vm, buf, result_len));
}

static Value replaceAllNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_STRING(argv[0]) || !IS_STRING(argv[1]) || !IS_STRING(argv[2])) {
        RUNTIME_ERR(vm, "replace_all expects three strings");
        return NIL_VAL;
    }
    ObjString* s = AS_STRING(argv[0]);
    ObjString* from = AS_STRING(argv[1]);
    ObjString* to = AS_STRING(argv[2]);

    if (from->length == 0)
        return argv[0];  // empty needle — undefined, return as-is

    // Pass 1: count occurrences
    int cnt = 0;
    char* p = s->chars;
    while ((p = strstr(p, from->chars)) != NULL) {
        cnt++;
        p += from->length;
    }

    if (cnt == 0) return argv[0];

    // Pass 2: build result
    int result_len = s->length + cnt * (to->length - from->length);
    char* buf = malloc(result_len + 1);
    if (buf == NULL) {
        RUNTIME_ERR(vm, "out of memory");
        return NIL_VAL;
    }

    char* src = s->chars;
    char* dst = buf;
    while ((p = strstr(src, from->chars)) != NULL) {
        int chunk = (int)(p - src);
        memcpy(dst, src, chunk);
        dst += chunk;
        memcpy(dst, to->chars, to->length);
        dst += to->length;
        src = p + from->length;
    }
    int tail = (int)(s->chars + s->length - src);
    memcpy(dst, src, tail + 1);  // +1 for '\0'

    return OBJ_VAL(takeString(vm, buf, result_len));
}

static Value splitNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_STRING(argv[0]) || !IS_STRING(argv[1])) {
        RUNTIME_ERR(vm, "split expects two strings");
        return NIL_VAL;
    }
    ObjString* s = AS_STRING(argv[0]);
    ObjString* delim = AS_STRING(argv[1]);

    int cnt = 1;
    char* p = s->chars;
    if (delim->length > 0) {
        while ((p = strstr(p, delim->chars)) != NULL) {
            cnt++;
            p += delim->length;
        }
    } else {
        cnt = s->length;  // empty delim → one char per segment
    }

    Seg* segs = malloc(cnt * sizeof(Seg));
    if (segs == NULL) {
        RUNTIME_ERR(vm, "out of memory");
        return NIL_VAL;
    }

    if (delim->length == 0) {
        for (int i = 0; i < cnt; i++) segs[i] = (Seg){s->chars + i, 1};
    } else {
        int ix = 0;
        const char* cur = s->chars;
        char* found;
        while ((found = strstr(cur, delim->chars)) != NULL) {
            segs[ix++] = (Seg){cur, (int)(found - cur)};
            cur = found + delim->length;
        }
        segs[ix] = (Seg){cur, (int)(s->chars + s->length - cur)};
    }

    // --- build list right-to-left, keeping head on the stack ---
    push(vm, NIL_VAL);  // initial head

    for (int i = cnt - 1; i >= 0; i--) {
        Value seg_str = OBJ_VAL(copyString(vm, segs[i].start, segs[i].len));
        push(vm, seg_str);             // protect seg_str during newPair
        Value old_head = peek(vm, 1);  // head is one below seg_str
        Value pair = OBJ_VAL(newPair(vm, seg_str, old_head));
        pop(vm);         // seg_str
        pop(vm);         // old head
        push(vm, pair);  // new head
    }

    Value head = peek(vm, 0);
    pop(vm);
    free(segs);
    return OBJ_VAL(newList(vm, (uint32_t)cnt, head));
}

static Value joinNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_LIST(argv[0]) || !IS_STRING(argv[1])) {
        RUNTIME_ERR(vm, "join expects a list and a string");
        return NIL_VAL;
    }
    ObjList* list = AS_LIST(argv[0]);
    ObjString* delim = AS_STRING(argv[1]);

    if (list->len == 0) return OBJ_VAL(copyString(vm, "", 0));

    // pass 1: total length
    int total = 0;
    Value node = list->head;
    bool first = true;
    while (!IS_NIL(node)) {
        ObjPair* pair = AS_PAIR(node);
        if (!IS_STRING(pair->first)) {
            RUNTIME_ERR(vm, "join: all list elements must be strings");
            return NIL_VAL;
        }
        if (!first) total += delim->length;
        total += AS_STRING(pair->first)->length;
        first = false;
        node = pair->second;
    }

    // pass 2: build
    char* buf = malloc(total + 1);
    if (buf == NULL) {
        RUNTIME_ERR(vm, "out of memory");
        return NIL_VAL;
    }

    char* dst = buf;
    node = list->head;
    first = true;
    while (!IS_NIL(node)) {
        ObjPair* pair = AS_PAIR(node);
        ObjString* elem = AS_STRING(pair->first);
        if (!first) {
            memcpy(dst, delim->chars, delim->length);
            dst += delim->length;
        }
        memcpy(dst, elem->chars, elem->length);
        dst += elem->length;
        first = false;
        node = pair->second;
    }
    *dst = '\0';

    return OBJ_VAL(takeString(vm, buf, total));
}

static Value parseIntNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_STRING(argv[0])) {
        RUNTIME_ERR(vm, "parse_int expects a string");
        return NIL_VAL;
    }
    ObjString* s = AS_STRING(argv[0]);
    if (s->length == 0) return OBJ_VAL(newError(vm, "parse_int: empty string"));

    char* end;
    long long val = strtoll(s->chars, &end, 10);
    if (end != s->chars + s->length)
        return OBJ_VAL(newError(vm, "parse_int: invalid integer"));
    return INT_VAL((int64_t)val);
}

static Value parseRealNative(VM* vm, int argc, Value* argv) {
    (void)argc;
    if (!IS_STRING(argv[0])) {
        RUNTIME_ERR(vm, "parse_real expects a string");
        return NIL_VAL;
    }
    ObjString* s = AS_STRING(argv[0]);
    if (s->length == 0) return OBJ_VAL(newError(vm, "parse_real: empty string"));

    char* end;
    double val = strtod(s->chars, &end);
    if (end != s->chars + s->length)
        return OBJ_VAL(newError(vm, "parse_real: invalid real"));
    return REAL_VAL((double)val);
}

static const NativeReg str_functions[] = {
    {"upper", 1, upperNative},
    {"lower", 1, lowerNative},
    {"trim", 1, trimNative},
    {"contains?", 2, containsNative},
    {"starts_with?", 2, startsWithNative},
    {"ends_with?", 2, endsWithNative},
    {"index_of", 2, indexOfNative},
    {"substr", 3, substrNative},
    {"replace", 3, replaceNative},
    {"replace_all", 3, replaceAllNative},
    {"split", 2, splitNative},
    {"join", 2, joinNative},
    {"parse_int", 1, parseIntNative},
    {"parse_real", 1, parseRealNative},
    {NULL, 0, NULL},
};

void registerStrNatives(VM* vm, ObjModule* module) {
    defineNatives(vm, module, str_functions);
}
