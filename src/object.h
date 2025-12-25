#ifndef liss_object_h
#define liss_object_h

#include "value.h"
#include <stdint.h>

// Forward-declare VM to break the circular dependency between vm.h and object.h
typedef struct VM VM;

// --- Base Object ---

typedef enum {
    OBJ_STRING,
    // Future object types will be added here
    // OBJ_FUNCTION,
    // OBJ_LIST,
    // etc.
} ObjType;

struct Obj {
    ObjType type;
    // 'next' field for the garbage collector to create a linked list of all objects.
    struct Obj* next;
};

// --- String Object ---

// Represents a string object on the heap.
typedef struct {
    Obj obj; // Must be the first field to allow casting
    int length;
    char* chars;
    uint32_t hash;
} ObjString;


// --- Helper Functions and Macros ---

// Helper function to check object type safely
static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

// Macro to get the ObjType from a Value
#define OBJ_TYPE(value)   (AS_OBJ(value)->type)

// Macros for checking the specific object type of a Value.
#define IS_STRING(value)  isObjType(value, OBJ_STRING)

// Macros for casting a Value to a specific object type pointer.
#define AS_STRING(value)  ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)


// --- Function Prototypes ---

// Allocates an ObjString on the heap and returns a pointer to it.
ObjString* takeString(VM* vm, char* chars, int length);

// Allocates a new ObjString by copying the given characters.
ObjString* copyString(VM* vm, const char* chars, int length);


#endif
