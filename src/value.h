#ifndef liss_value_h
#define liss_value_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// Forward-declare Obj so Value can have a pointer to it.
typedef struct Obj Obj;

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_INT,
    VAL_REAL,
    VAL_OBJ  // A heap-allocated object
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        int64_t integer;
        double real;
        Obj* obj;
    } as;
} Value;

// Macros to check the type of a Value
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_INT(value) ((value).type == VAL_INT)
#define IS_REAL(value) ((value).type == VAL_REAL)
#define IS_OBJ(value) ((value).type == VAL_OBJ)

#define IS_NUMERIC(value) (IS_INT(value) || IS_REAL(value))

// Macros to "unwrap" a Value to its C type
#define AS_BOOL(value) ((value).as.boolean)
#define AS_INT(value) ((value).as.integer)
#define AS_REAL(value) ((value).as.real)
#define AS_OBJ(value) ((value).as.obj)

// Macros to create a Value from a C type
#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((Value){VAL_NIL, {.integer = 0}})  // Payload is irrelevant
#define INT_VAL(value) ((Value){VAL_INT, {.integer = value}})
#define REAL_VAL(value) ((Value){VAL_REAL, {.real = value}})
#define OBJ_VAL(object) ((Value){VAL_OBJ, {.obj = (Obj*)object}})

#define DEBUG_VALUE(fmt, value)          \
    do {                                 \
        char* strv = sprintValue(value); \
        DEBUG_LOG(fmt, strv);            \
        free(strv);                      \
    } while (0)

bool valuesEqual(Value a, Value b);

char* sprintValue(Value value);

bool isFalsey(Value value);

#endif
