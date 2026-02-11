#include "value.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "object.h"

bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:
            return true;  // All nils are equal.
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);
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
                        return false;
                }
            // TODO: Deep comparison for certain object types (e.g., strings).
            return AS_OBJ(a) == AS_OBJ(b);  // Compare object pointers.
    }

    return false;  // Unreachable.
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
            APPEND_TO_BUFFER("nil");
            break;
        case VAL_NUMBER: {
            APPEND_TO_BUFFER("%g", AS_NUMBER(value));
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
