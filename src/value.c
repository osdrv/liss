#include "value.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "hamt.h"
#include "object.h"

// forward-declaration
typedef struct {
    Value key;
    Value val;
} DictPair;

bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:
            return true;  // All nils are equal.
        case VAL_INT:
            return AS_INT(a) == AS_INT(b);
        case VAL_REAL:
            return AS_REAL(a) == AS_REAL(b);
        case VAL_OBJ:
            if (OBJ_TYPE(a) != OBJ_TYPE(b)) {
                return false;  // Different object types can't be equal.
            }
            switch
                OBJ_TYPE(a) {
                    case OBJ_STRING:
                        ObjString* strA = AS_STRING(a);
                        ObjString* strB = AS_STRING(b);
                        if (strA->length != strB->length) return false;
                        return memcmp(strA->chars, strB->chars, strA->length) ==
                               0;
                    default:
                        break;
                }
            // Fallback to pointer comparison for other object types (e.g.,
            // functions).
            return AS_OBJ(a) == AS_OBJ(b);  // Compare object pointers.
    }

    return false;  // Unreachable.
}

static int cmpValues(Value a, Value b) {
    // Sort by the type tag
    if (a.type != b.type) return a.type - b.type;

    switch (a.type) {
        case VAL_BOOL:
            return AS_BOOL(a) - AS_BOOL(b);
        case VAL_NIL:
            return 0;
        case VAL_INT:
            return (AS_INT(a) < AS_INT(b)) ? -1 : (AS_INT(a) > AS_INT(b));
        case VAL_REAL:
            return (AS_REAL(a) < AS_REAL(b)) ? -1 : (AS_REAL(a) > AS_REAL(b));
        case VAL_OBJ: {
            if (OBJ_TYPE(a) != OBJ_TYPE(b)) return OBJ_TYPE(a) - OBJ_TYPE(b);
            if (IS_STRING(a)) {
                return strcmp(AS_CSTRING(a), AS_CSTRING(b));
            }
            return (AS_OBJ(a) < AS_OBJ(b)) ? -1 : (AS_OBJ(a) > AS_OBJ(b));
        }
    }
    return 0;
}

static int cmpDictPairs(const void* a, const void* b) {
    return cmpValues(((const DictPair*)a)->key, ((const DictPair*)b)->key);
}

typedef struct {
    DictPair* entries;
    int count;
    int cap;
} CollectCtx;

static void collectPair(Value key, Value val, void* ctx_) {
    CollectCtx* ctx = ctx_;
    if (ctx->count == ctx->cap) {
        ctx->cap *= 2;
        ctx->entries = realloc(ctx->entries, sizeof(DictPair) * ctx->cap);
    }
    ctx->entries[ctx->count++] = (DictPair){key, val};
}

char* sprintValue(Value value) {
    char* buffer = NULL;
    size_t buffer_size = 0;
    size_t offset = 0;

#define APPEND_TO_BUFFER(fmt, ...)                                     \
    do {                                                               \
        int needed = snprintf(NULL, 0, fmt, ##__VA_ARGS__);            \
        if (offset + needed + 1 > buffer_size) {                       \
            buffer_size = (buffer_size == 0) ? 256 : buffer_size * 2;  \
            buffer = realloc(buffer, buffer_size);                     \
        }                                                              \
        offset += snprintf(buffer + offset, buffer_size - offset, fmt, \
                           ##__VA_ARGS__);                             \
    } while (0)

    switch (value.type) {
        case VAL_BOOL:
            APPEND_TO_BUFFER("%s", AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL:
            APPEND_TO_BUFFER("null");
            break;
        case VAL_INT: {
            APPEND_TO_BUFFER("%i", AS_INT(value));
            break;
        }
        case VAL_REAL: {
            APPEND_TO_BUFFER("%g", AS_REAL(value));
            break;
        }
        case VAL_OBJ:
            switch (OBJ_TYPE(value)) {
                case OBJ_STRING:
                    APPEND_TO_BUFFER("\"%s\"", AS_CSTRING(value));
                    break;
                case OBJ_FUNCTION:
                    APPEND_TO_BUFFER("<fn %s>",
                                     AS_FUNCTION(value)->name
                                         ? AS_FUNCTION(value)->name->chars
                                         : "<code>");
                    break;
                case OBJ_NATIVE:
                    APPEND_TO_BUFFER("<native fn %s>",
                                     AS_NATIVE(value)->name->chars);
                    break;
                case OBJ_ERROR:
                    APPEND_TO_BUFFER("<error: %s>",
                                     AS_ERROR(value)->message->chars);
                    break;
                case OBJ_LIST: {
                    APPEND_TO_BUFFER("[");
                    ObjList* list = AS_LIST(value);
                    Value current = list->head;
                    for (uint32_t i = 0; i < list->len; i++) {
                        if (!IS_PAIR(current)) {
                            APPEND_TO_BUFFER("<corrupt list>");
                            break;
                        }
                        ObjPair* pair =
                            AS_PAIR(current);  // Ensure it's a pair for proper
                                               // list structure
                        char* elem_str = sprintValue(pair->first);
                        APPEND_TO_BUFFER("%s", elem_str);
                        free(elem_str);
                        if (i < list->len - 1) {
                            APPEND_TO_BUFFER(" ");
                        }
                        if (IS_PAIR(current)) {
                            current = pair->second;
                        } else {
                            break;  // Not a proper list, stop here.
                        }
                    }
                    APPEND_TO_BUFFER("]");
                    break;
                }
                case OBJ_PAIR: {
                    ObjPair* pair = AS_PAIR(value);
                    char* first_str = sprintValue(pair->first);
                    char* second_str = sprintValue(pair->second);
                    APPEND_TO_BUFFER("(%s . %s)", first_str, second_str);
                    free(first_str);
                    free(second_str);
                    break;
                }
                case OBJ_DICT: {
                    ObjDict* dict = AS_DICT(value);
                    APPEND_TO_BUFFER("(dict");
                    CollectCtx ctx = {malloc(sizeof(DictPair) * 8), 0, 8};
                    hamtEach(dict->root, collectPair, &ctx);
                    qsort(ctx.entries, ctx.count, sizeof(DictPair),
                          cmpDictPairs);
                    for (int i = 0; i < ctx.count; i++) {
                        char* k = sprintValue(ctx.entries[i].key);
                        char* v = sprintValue(ctx.entries[i].val);
                        APPEND_TO_BUFFER(" (%s . %s)", k, v);
                        free(k);
                        free(v);
                    }
                    free(ctx.entries);
                    APPEND_TO_BUFFER(")");
                    break;
                }
                default:
                    APPEND_TO_BUFFER("<object>");
            }
            break;
        default:
            APPEND_TO_BUFFER("<unknown value>");
    }
#undef APPEND_TO_BUFFER
    buffer[offset] = '\0';
    return buffer;
}

bool isFalsey(Value value) {
    return (IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)));
}
