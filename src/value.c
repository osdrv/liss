#include "value.h"

#include <stdio.h>

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
            return AS_OBJ(a) == AS_OBJ(b);  // Compare object pointers.
    }

    return false;  // Unreachable.
}

void printValue(Value value) {
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_NUMBER:
            printf("Number: %g", AS_NUMBER(value));
            break;
        case VAL_OBJ:
            switch (OBJ_TYPE(value)) {
                case OBJ_STRING:
                    printf("\"%s\"", AS_CSTRING(value));
                    break;
                case OBJ_FUNCTION:
                    printf("<fn %s>", AS_FUNCTION(value)->name
                                          ? AS_FUNCTION(value)->name->chars
                                          : "code");
                    break;
            }
            break;
    }
}

bool isFalsey(Value value) {
    return (IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)));
}
