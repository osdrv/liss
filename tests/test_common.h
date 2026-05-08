#ifndef liss_tests_test_common_h
#define liss_tests_test_common_h

#include "value.h"
#include "object.h"
#include "minunit.h"

typedef enum {
    EXPECT_BOOL,
    EXPECT_NIL,
    EXPECT_INT,
    EXPECT_REAL,
    EXPECT_STRING,
    EXPECT_LIST,
    EXPECT_PAIR,
    EXPECT_ERROR,
} ExpectedValueType;

typedef struct {
    ExpectedValueType type;
    union {
        bool boolean;
        int64_t integer;
        double real;
        const char* string;
    } as;
} ExpectedValue;

static char* assert_int(Value value, int64_t expected) {
    mu_assert("Value is not an integer.", IS_INT(value));
    mu_assert("Integer value does not match expected.",
              AS_INT(value) == expected);
    return NULL;
}

static char* assert_real(Value value, double expected) {
    mu_assert("Value is not a real number.", IS_REAL(value));
    mu_assert("Real value does not match expected.",
              AS_REAL(value) == expected);
    return NULL;
}

static char* assert_string(Value value, const char* expected) {
    mu_assert("Value is not an object.", IS_OBJ(value));
    mu_assert("Object is not a string.", OBJ_TYPE(value) == OBJ_STRING);
    ObjString* str = AS_STRING(value);
    mu_assert("String length does not match expected.",
              str->length == (int)strlen(expected));
    mu_assert("String contents do not match expected.",
              memcmp(str->chars, expected, str->length) == 0);
    return NULL;
}

static char* assert_bool(Value value, bool expected) {
    mu_assert("Value is not a boolean.", IS_BOOL(value));
    mu_assert("Boolean value does not match expected.",
              AS_BOOL(value) == expected);
    return NULL;
}

static char* assert_nil(Value value) {
    mu_assert("Value is not null.", IS_NIL(value));
    return NULL;
}

static char* assert_list(Value value, const char* expected_str) {
    mu_assert("Value is not an object.", IS_OBJ(value));
    mu_assert("Object is not a list.", OBJ_TYPE(value) == OBJ_LIST);
    ObjList* list = AS_LIST(value);

    // Convert the list to a string representation for comparison
    char* str = sprintValue(value);
    mu_assert("List string representation does not match expected.",
              strcmp(str, expected_str) == 0);
    free(str);
    return NULL;
}

static char* assert_pair(Value value, const char* expected_str) {
    mu_assert("Value is not an object.", IS_OBJ(value));
    mu_assert("Object is not a pair.", OBJ_TYPE(value) == OBJ_PAIR);
    ObjPair* pair = AS_PAIR(value);

    char* str = sprintValue(value);
    mu_assert("Pair string representation does not match expected.",
        strcmp(str, expected_str) ==  0);
    free(str);
    return NULL;
}

// For now, errors have no metadata, so we simply check the message.
static char* assert_error(Value value, const char* expected) {
    mu_assert("Value is not an object.", IS_OBJ(value));
    mu_assert("Object is not an error.", OBJ_TYPE(value) == OBJ_ERROR);
    ObjError* error = AS_ERROR(value);
    mu_assert("Error message does not match expected.",
              strcmp(error->message->chars, expected) == 0);
    return NULL;
}

#endif // liss_tests_test_common_h
